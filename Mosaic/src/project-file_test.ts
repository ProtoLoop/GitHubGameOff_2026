import { strict as assert } from "node:assert";
import { createDocument, cloneDocument, sameContent, type ImageAsset, type PropAsset } from "./document.ts";
import { readProject, writeProject } from "./project-file.ts";
import { cellMasks, terrainSourceOffset } from "./placement.ts";

function image(kind: "tile" | "mask", filename: string): ImageAsset
{
    return { id: `assets/${kind === "tile" ? "tiles" : "masks"}/${filename}`, kind, label: filename,
        blob: new Blob(["image bytes"]), dimension: 256, url: "blob:test",
        pixels: new Uint8ClampedArray(0), coverage: new Uint8Array(0) };
}
const tile = image("tile", "renamed # tile.png");
const mask = image("mask", "mask.png");

function fixture()
{
    const project = createDocument([tile], [mask]);
    project.layers[1].painted[100] = true;
    project.maskMode = 0;
    project.view = { zoom: 0.5, scrollLeft: 100, scrollTop: 200, showGrid: false };
    return project;
}

Deno.test("sparse occupancy round-trips with identical derived rendering choices", async () => {
    const project = fixture();
    const blob = await writeProject(project);
    assert.equal(blob.type, "application/json");
    const text = await blob.text();
    assert(text.includes('\n  "version": 21,'));
    assert(!text.includes("image bytes"));
    const data = JSON.parse(text);
    assert.deepEqual(Object.keys(data), ["format", "version", "gridSize", "tileSheets", "maskSheets", "maskMode", "propAssets", "props", "layers", "editor"]);
    assert.deepEqual(data.tileSheets, ["/" + tile.id]);
    assert.deepEqual(data.maskSheets, ["/" + mask.id]);
    assert.deepEqual(data.layers[0], { sheet: 0, cells: [] });
    assert.equal(data.maskMode, 0);
    assert.equal(data.editor.maskMode, undefined);
    assert.deepEqual(data.layers[1], { sheet: 0,
        cells: [100] });
    assert.equal(data.editor.layers, undefined);
    const loaded = await readProject(blob, [tile, mask]);
    assert.equal(loaded.layers[1].painted[99], false);
    assert.equal(loaded.layers[1].painted[100], true);
    for(let cell = 0; cell < 4096; cell++)
    {
        assert.deepEqual(terrainSourceOffset(loaded, loaded.layers[1], cell, 17, 29), terrainSourceOffset(project, project.layers[1], cell, 17, 29));
        assert.deepEqual(cellMasks(loaded, loaded.layers[1], cell), cellMasks(project, project.layers[1], cell));
    }
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), data);
});

Deno.test("level JSON larger than 16 MiB loads and can be saved again", async () => {
    const original = await writeProject(fixture());
    const large = new Blob([" ".repeat(16 * 1024 * 1024), original]);
    assert(large.size > 16 * 1024 * 1024);
    const loaded = await readProject(large, [tile, mask]);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), JSON.parse(await original.text()));
});

Deno.test("file asset paths require a project-root slash while discovered IDs remain relative", async () => {
    const prop: PropAsset = { kind: "prop", id: "assets/props/trees #/tree%20.png", label: "trees #/tree%20.png",
        blob: new Blob(), url: "blob:prop", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 1, scale: 0.25, collisionRects: [] };
    const project = fixture();
    project.propAssets = [prop];
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert.deepEqual(data.tileSheets, ["/" + tile.id]);
    assert.deepEqual(data.maskSheets, ["/" + mask.id]);
    assert.equal(data.propAssets[0].texPath, "/" + prop.id);
    const available = [tile, mask, prop];
    const loaded = await readProject(blob, available);
    assert.equal(loaded.tileSheets[0], tile);
    assert.equal(loaded.masks[0], mask);
    assert.equal(loaded.propAssets[0].id, prop.id);
    assert.equal(loaded.propAssets[0].url, prop.url);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), data);
    for(const [index, asset] of available.entries())
    {
        for(const path of [asset.id, "//" + asset.id, "/projects/project2/" + asset.id,
            "https://example.com/" + asset.id, "/./" + asset.id, "/../" + asset.id,
            "/" + asset.id.replace("assets/", "assets//"), "/" + asset.id.replace("assets/", "assets/../assets/")])
        {
            const invalid = structuredClone(data);
            if(index === 0) { invalid.tileSheets[0] = path; }
            else if(index === 1) { invalid.maskSheets[0] = path; }
            else { invalid.propAssets[0].texPath = path; }
            await assert.rejects(() => readProject(new Blob([JSON.stringify(invalid)]), available));
        }
    }
});

Deno.test("the root slash preserves the existing 1024-character relative asset path limit", async () => {
    const longTile = { ...tile, id: "assets/tiles/" + "a/".repeat(500) + "longnam.png", label: "longnam.png" };
    assert.equal(longTile.id.length, 1024);
    const project = createDocument([longTile], [mask]);
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert.equal(data.tileSheets[0].length, 1025);
    assert.equal((await readProject(blob, [longTile, mask])).tileSheets[0], longTile);
    data.tileSheets[0] += "a";
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [longTile, mask]), /Invalid project asset path/);
});

Deno.test("newly discovered sheets append without changing occupancy or catalog indices", async () => {
    const project = fixture();
    const blob = await writeProject(project);
    const extraMask = image("mask", "aaa.png");
    const extraTile = image("tile", "aaa.png");
    const loaded = await readProject(blob, [extraMask, extraTile, tile, mask]);
    assert.deepEqual(loaded.tileSheets, [tile, extraTile]);
    assert.deepEqual(loaded.masks, [mask, extraMask]);
    assert.deepEqual(loaded.layers, project.layers);
    assert.deepEqual(loaded.view, project.view);
    const saved = JSON.parse(await (await writeProject(loaded)).text());
    assert.deepEqual(saved.layers, JSON.parse(await blob.text()).layers);
});

Deno.test("invalid sparse records and missing assets fail before installation", async () => {
    const blob = await writeProject(fixture());
    await assert.rejects(() => readProject(blob, [mask]), /Missing image/);
    const original = JSON.parse(await blob.text());
    type File = typeof original;
    const changes: ((data: File) => void)[] = [
        (data) => { data.version = 2; },
        (data) => { data.version = 8; },
        (data) => { data.version = 9; },
        (data) => { data.version = 10; },
        (data) => { data.version = 11; },
        (data) => { data.version = 12; },
        (data) => { data.version = 13; },
        (data) => { data.version = 14; },
        (data) => { data.version = 15; },
        (data) => { data.version = 16; },
        (data) => { data.version = 17; },
        (data) => { data.version = 18; },
        (data) => { data.version = 19; },
        (data) => { data.version = 20; },
        (data) => { data.editor = null; },
        (data) => { data.maskSheets.push("/" + mask.id); },
        (data) => { data.tileSheets.push("/" + tile.id); },
        (data) => { data.tileSheets = ["/" + mask.id]; },
        (data) => { data.maskSheets = ["/" + tile.id]; },
        (data) => { data.gridSize = 513; },
        (data) => { data.layers[0].cells = [0]; },
        (data) => { data.layers[1].sheet = 1; },
        (data) => { data.layers[1].cells.push(data.layers[1].cells[0]); },
        (data) => { data.layers[1].cells[0] = 4096; },
        (data) => { data.layers[1].cells[0] = -1; },
        (data) => { data.layers[1].cells[0] = 1.5; },
        (data) => { data.layers[1].cells[0] = "100"; },
        (data) => { data.layers[1].cells = [null]; },
        (data) => { data.layers[1].cells = [{ cell: 100 }]; },
        (data) => { delete data.layers[1].cells; },
        (data) => { data.editor.activeLayer = 2; },
        (data) => { data.editor.activeLayer = -1; },
        (data) => { data.editor.activeLayer = 0.5; },
        (data) => { delete data.editor.activeLayer; },
        (data) => { data.maskMode = 1; },
        (data) => { data.editor.view.zoom = 99; }
    ];
    for(const change of changes)
    {
        const data = structuredClone(original);
        change(data);
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask]));
    }
});

Deno.test("session IDs are omitted and reordered layers retain rendering and selection on reload", async () => {
    const project = fixture();
    project.layers[1] = { ...project.layers[1], id: 42 };
    project.layers.push({ ...project.layers[1], id: 99, painted: Array(4096).fill(true) });
    project.activeLayerId = 42;
    project.nextLayerId = 100;
    [project.layers[1], project.layers[2]] = [project.layers[2], project.layers[1]];
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert(data.layers.every((layer: object) => !("id" in layer)));
    assert.deepEqual(Object.keys(data.editor), ["activeLayer", "view"]);
    assert.equal(data.editor.activeLayer, 2);
    const loaded = await readProject(blob, [tile, mask]);
    assert.equal(loaded.activeLayerId, loaded.layers[2].id);
    assert.equal(loaded.nextLayerId, 3);
    for(let index = 0; index < project.layers.length; index++)
    {
        assert.deepEqual(loaded.layers[index].painted, project.layers[index].painted);
        for(let cell = 0; cell < 4096; cell++)
        {
            assert.deepEqual(terrainSourceOffset(loaded, loaded.layers[index], cell, 17, 29), terrainSourceOffset(project, project.layers[index], cell, 17, 29));
            if(index !== 0)
            {
                assert.deepEqual(cellMasks(loaded, loaded.layers[index], cell), cellMasks(project, project.layers[index], cell));
            }
        }
    }
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), data);
});

Deno.test("whole-texture dimensions are derived and discovered textures must match", async () => {
    for(const dimension of [64, 192, 256, 1024, 4096])
    {
        const material = { ...tile, dimension };
        const project = createDocument([material], [mask]);
        project.layers[1].painted[65] = true;
        const blob = await writeProject(project);
        assert.equal("tileSize" in JSON.parse(await blob.text()), false);
        const loaded = await readProject(blob, [material, mask]);
        assert.deepEqual(loaded.layers, project.layers);
        const extra = { ...tile, id: "assets/tiles/extra.png", label: "extra.png", dimension };
        const expanded = await readProject(blob, [material, mask, extra]);
        assert.deepEqual(expanded.tileSheets, [material, extra]);
        await assert.rejects(() => readProject(blob, [material, mask,
            { ...extra, dimension: dimension === 256 ? 512 : 256 }]), /extra.png/);
    }
});

Deno.test("props round-trip with catalog order, transforms and placement order intact", async () => {
    const asset: PropAsset = { id: "assets/props/stone #1.png", kind: "prop", label: "stone #1.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const extra = { ...asset, id: "assets/props/extra.png", label: "extra.png" };
    const project = fixture();
    project.propAssets = [asset];
    project.props = [{ id: 5, asset, x: 2.5, y: 3.25, rotation: 45 },
        { id: 9, asset, x: 1, y: 2, rotation: 270 }];
    project.nextPropId = 10;
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert.deepEqual(data.props, [
        { index: 0, pos: { x: 2.5, y: 3.25 }, rot: 45 },
        { index: 0, pos: { x: 1, y: 2 }, rot: 270 }
    ]);
    const loaded = await readProject(blob, [tile, mask, extra, asset]);
    assert.deepEqual(loaded.propAssets, [asset, extra]);
    assert.deepEqual(loaded.props.map(({ x, y, rotation }) => ({ x, y, rotation })),
        project.props.map(({ x, y, rotation }) => ({ x, y, rotation })));
    assert.equal(loaded.nextPropId, 2);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()).props, data.props);
    await assert.rejects(() => readProject(blob, [tile, mask]));
    for(const corrupt of [
        (d: typeof data) => { d.version = 7; },
        (d: typeof data) => { delete d.props; },
        (d: typeof data) => { d.props[0].asset = d.props[0].index; delete d.props[0].index; },
        (d: typeof data) => { d.props[0].rotation = d.props[0].rot; delete d.props[0].rot; },
        (d: typeof data) => { d.props[0].index = 1; },
        (d: typeof data) => { d.props[0].pos.x = "200"; },
        (d: typeof data) => { d.props[0].pos.y = null; },
        (d: typeof data) => { delete d.props[0].pos; },
        (d: typeof data) => { d.props[0].pos = null; },
        (d: typeof data) => { d.props[0].pos = []; },
        (d: typeof data) => { d.props[0].pos = 1; },
        (d: typeof data) => { delete d.props[0].pos.x; },
        (d: typeof data) => { delete d.props[0].pos.y; },
        (d: typeof data) => { Object.assign(d.props[0], d.props[0].pos); delete d.props[0].pos; },
        (d: typeof data) => { d.props[0].rot = 360; },
        (d: typeof data) => { d.propAssets = [tile.id]; }
    ])
    {
        const invalid = structuredClone(data);
        corrupt(invalid);
        await assert.rejects(() => readProject(new Blob([JSON.stringify(invalid)]), [tile, mask, asset]));
    }
});

Deno.test("prop metadata defaults independently and round-trips without mutating decoded assets", async () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const extra = { ...asset, id: "assets/props/new.png", label: "new.png" };
    const project = fixture();
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 1, y: 2, rotation: 90 }];
    project.nextPropId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    for(const fields of [{}, { pivotX: 0 }, { pivotY: 1 }, { scale: 0.25 }, { pivotX: 0.25, pivotY: 1, scale: 2 }])
    {
        data.propAssets = [{ texPath: "/" + asset.id, pivot: { x: fields.pivotX, y: fields.pivotY }, scale: fields.scale }];
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, extra, asset]);
        const expected = { pivotX: 0.5, pivotY: 0.5, scale: 1, ...fields, collisionRects: [] };
        assert.deepEqual(loaded.propAssets[0], { ...asset, ...expected });
        assert.equal(loaded.propAssets[1], extra);
        assert.equal(loaded.props[0].asset, loaded.propAssets[0]);
        assert.equal(loaded.props[0].asset.image, asset.image);
        assert.deepEqual([asset.pivotX, asset.pivotY, asset.scale], [0.5, 0.5, 1]);
        const saved = await writeProject(loaded);
        assert.deepEqual(JSON.parse(await saved.text()).propAssets, [
            { texPath: "/" + asset.id, pivot: { x: expected.pivotX, y: expected.pivotY }, scale: expected.scale, srcRect: [], collisionRects: [] },
            { texPath: "/" + extra.id, pivot: { x: 0.5, y: 0.5 }, scale: 1, srcRect: [], collisionRects: [] }
        ]);
        const again = await readProject(saved, [tile, mask, extra, asset]);
        assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), JSON.parse(await saved.text()));
    }
    for(const entry of [asset.id, {}, null, { texPath: "/" + tile.id }, { texPath: 42 },
        ...["x", "y"].flatMap((key) => [-0.01, 1.01, null, "0.5", true].map((value) => ({ texPath: "/" + asset.id, pivot: { [key]: value } }))),
        ...[0, -1, null, "1", false].map((scale) => ({ texPath: "/" + asset.id, scale }))])
    {
        data.propAssets = [entry];
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
    // Distinct metadata objects must not allow duplicate texture identities.
    data.propAssets = [{ texPath: "/" + asset.id }, { texPath: "/" + asset.id, scale: 2 }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    data.propAssets = [{ texPath: "/" + asset.id, scale: "overflow" }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data).replace('"overflow"', "1e400")]), [tile, mask, asset]));
});

Deno.test("nested paths round-trip and newly discovered catalogs append without changing references", async () => {
    const first = image("tile", "woods/ground.png");
    const second = image("tile", "fields/ground.png");
    const extra = image("tile", "a/ground.png");
    const prop: PropAsset = { kind: "prop", id: "assets/props/old #/stone%20.png", label: "old #/stone%20.png",
        blob: new Blob(), url: "", width: 20, height: 40, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 20, h: 40 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const newProp = { ...prop, id: "assets/props/new/stone%20.png", label: "new/stone%20.png" };
    const project = createDocument([first, second], [mask]);
    project.propAssets = [prop];
    project.props = [{ id: 0, asset: prop, x: 1, y: 2, rotation: 0 }];
    project.nextPropId = 1;
    const blob = await writeProject(project);
    const loaded = await readProject(blob, [extra, second, first, mask, newProp, prop]);
    assert.deepEqual(loaded.tileSheets, [first, second, extra]);
    assert.deepEqual(loaded.propAssets, [prop, newProp]);
    assert.equal(loaded.layers[1].sheet, second);
    assert.equal(loaded.props[0].asset, loaded.propAssets[0]);
    assert.equal(loaded.props[0].asset.image, prop.image);
    const saved = JSON.parse(await (await writeProject(loaded)).text());
    assert.deepEqual(saved.tileSheets, [first, second, extra].map((asset) => "/" + asset.id));
    assert.deepEqual(saved.propAssets, [prop, newProp].map((asset) => ({
        texPath: "/" + asset.id, pivot: { x: 0.5, y: 0.5 }, scale: 1, srcRect: [], collisionRects: []
    })));
    const again = await readProject(await writeProject(loaded), [extra, second, first, mask, newProp, prop]);
    assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), saved);
    await assert.rejects(() => readProject(blob, [first, second, mask, newProp]), /Missing image/);
});

Deno.test("prop coordinates round on load and save without accepting invalid bounds", async () => {
    const project = fixture();
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone",
        blob: new Blob(), url: "", width: 20, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 20, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 12.12349, y: 31.9998, rotation: 45 }];
    project.nextPropId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    assert.deepEqual(data.props, [{ index: 0, pos: { x: 12.123, y: 32 }, rot: 45 }]);
    assert.equal(project.props[0].x, 12.12349);
    data.props[0].pos.x = 0.0004;
    data.props[0].pos.y = 23.56789;
    const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
    assert.equal(loaded.props[0].x, 0);
    assert.equal(loaded.props[0].y, 23.568);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()).props,
        [{ index: 0, pos: { x: 0, y: 23.568 }, rot: 45 }]);
    for(const invalid of [-32.0001, 32.0001])
    {
        data.props[0].pos.x = invalid;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
});

Deno.test("variable map sizes round-trip and enforce size-specific sparse indices and prop bounds", async () => {
    const asset: PropAsset = { id: "assets/props/a.png", kind: "prop", label: "a",
        blob: new Blob(), url: "", width: 10, height: 10, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 10, h: 10 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    for(const size of [2, 4, 8, 16, 32, 64, 128, 256, 512])
    {
        const project = fixture();
        project.gridSize = size;
        project.layers = project.layers.map((layer, index) => ({ ...layer, painted: Array(size * size).fill(index === 0) }));
        project.layers[1].painted[size * size - 1] = true;
        project.propAssets = [asset];
        project.props = [{ id: 0, asset, x: -size / 2, y: size / 2, rotation: 0 }];
        project.nextPropId = 1;
        const blob = await writeProject(project);
        const data = JSON.parse(await blob.text());
        assert.equal(data.version, 21);
        assert.deepEqual(data.layers[1].cells, [size * size - 1]);
        const loaded = await readProject(blob, [tile, mask, asset]);
        assert.equal(loaded.gridSize, size);
        assert.deepEqual(loaded.layers, project.layers);
        assert.deepEqual(loaded.props, project.props);
        assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), data);
        for(const corrupt of [
            (value: typeof data) => { value.layers[1].cells = [size * size]; },
            (value: typeof data) => { value.props[0].pos.x = -size / 2 - 0.0001; },
            (value: typeof data) => { value.props[0].pos.y = size / 2 + 0.0001; },
            (value: typeof data) => { value.gridSize = 0; },
            (value: typeof data) => { value.gridSize = 513; },
            (value: typeof data) => { value.gridSize = 1.5; },
            (value: typeof data) => { value.gridSize = 1; },
            (value: typeof data) => { value.gridSize = 3; },
            (value: typeof data) => { value.version = 12; }
        ])
        {
            const invalid = structuredClone(data);
            corrupt(invalid);
            await assert.rejects(() => readProject(new Blob([JSON.stringify(invalid)]), [tile, mask, asset]));
        }
    }
});

Deno.test("collision rectangles default, validate and round-trip independently of decoded assets", async () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const project = fixture();
    project.propAssets = [asset];
    const data = JSON.parse(await (await writeProject(project)).text());
    const collisionRects = [{ x: -0.2, y: 0.75, w: 1.5, h: 0.2 }, { x: 0.3, y: 0.8, w: 0.4, h: 0.5 }];
    for(const value of [undefined, [], collisionRects])
    {
        data.propAssets[0].collisionRects = value;
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
        assert.deepEqual(loaded.propAssets[0].collisionRects, value ?? []);
        assert.deepEqual(asset.collisionRects, []);
        const saved = JSON.parse(await (await writeProject(loaded)).text());
        assert.deepEqual(saved.propAssets[0].collisionRects, value ?? []);
        assert(!("bounds" in saved.propAssets[0]));
        assert(!("collider" in saved.propAssets[0]));
        assert.deepEqual(saved.propAssets[0].pivot, { x: 0.5, y: 0.5 });
        assert(!("pivotX" in saved.propAssets[0]));
    }
    for(const value of [null, {}, 1, [null], [[]], [{}],
        ...["x", "y", "w", "h"].flatMap((key) => [undefined, null, "1", true].map((bad) => [{ ...collisionRects[0], [key]: bad }])),
        ...["w", "h"].flatMap((key) => [0, -1].map((bad) => [{ ...collisionRects[0], [key]: bad }]))])
    {
        data.propAssets[0].collisionRects = value;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
    for(const key of ["x", "y", "w", "h"])
    {
        for(const bad of [NaN, Infinity, -Infinity])
        {
            project.propAssets = [{ ...asset, collisionRects: [{ ...collisionRects[0], [key]: bad }] }];
            assert.throws(() => writeProject(project), /collision rectangles/);
        }
    }
    data.propAssets[0].collisionRects = [];
    for(const pivot of [null, [], 1, "center"])
    {
        data.propAssets[0].pivot = pivot;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
});

Deno.test("source rectangles default to the full texture and round-trip without changing decoded assets", async () => {
    const asset: PropAsset = { kind: "prop", id: "assets/props/atlas.png", label: "atlas",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 }, pivotX: 0.25, pivotY: 1, scale: 2, collisionRects: [] };
    const project = fixture();
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 1, y: 2, rotation: 90 }];
    project.nextPropId = 1;
    const before = cloneDocument(project);
    const data = JSON.parse(await (await writeProject(project)).text());
    for(const srcRect of [undefined, [], { x: 0, y: 0, w: 80, h: 20 },
        { x: 0, y: 0, w: 79, h: 20 }, { x: 0, y: 0, w: 80, h: 19 },
        { x: 32, y: 4, w: 48, h: 16 }, { x: 79, y: 19, w: 1, h: 1 }])
    {
        data.propAssets[0].srcRect = srcRect;
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
        const fullTexture = srcRect === undefined || Array.isArray(srcRect)
            || (srcRect.x === 0 && srcRect.y === 0 && srcRect.w === 80 && srcRect.h === 20);
        assert.deepEqual(loaded.propAssets[0].srcRect, fullTexture ? asset.srcRect : srcRect);
        assert.equal(loaded.props[0].asset, loaded.propAssets[0]);
        assert.equal(loaded.props[0].asset.image, asset.image);
        assert.deepEqual(loaded.propAssets[0].collisionRects, asset.collisionRects);
        assert.equal(loaded.propAssets[0].pivotX, asset.pivotX);
        const saved = await writeProject(loaded);
        assert.deepEqual(JSON.parse(await saved.text()).propAssets[0].srcRect, fullTexture ? [] : srcRect);
        const again = await readProject(saved, [tile, mask, asset]);
        assert.deepEqual(again.propAssets[0], loaded.propAssets[0]);
        assert.equal(cloneDocument(loaded).propAssets[0], loaded.propAssets[0]);
    }
    assert(sameContent(project, before));
    assert.deepEqual(asset.srcRect, { x: 0, y: 0, w: 80, h: 20 });
});

Deno.test("source rectangles reject malformed, fractional and out-of-texture regions on load and save", async () => {
    const asset: PropAsset = { kind: "prop", id: "assets/props/atlas.png", label: "atlas",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 }, pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const project = fixture();
    project.propAssets = [asset];
    const data = JSON.parse(await (await writeProject(project)).text());
    const rect = { x: 5, y: 2, w: 10, h: 8 };
    const invalid = [null, [0, 0, 80, 20], [{}], {}, "rectangle", ...["x", "y", "w", "h"].flatMap((key) =>
        [undefined, null, "1", true, -1, 0.5, NaN, Infinity, Number.MAX_SAFE_INTEGER].map((value) => ({ ...rect, [key]: value }))),
        { ...rect, w: 0 }, { ...rect, h: 0 }, { ...rect, x: 71 }, { ...rect, y: 13 }];
    for(const srcRect of invalid)
    {
        data.propAssets[0].srcRect = srcRect;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
        project.propAssets = [{ ...asset, srcRect: srcRect as PropAsset["srcRect"] }];
        assert.throws(() => writeProject(project), /source rectangle/);
    }
    // JSON overflow must fail as well, independently of JSON.stringify converting Infinity to null.
    data.propAssets[0].srcRect = { ...rect, x: "overflow" };
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data).replace('"overflow"', "1e400")]), [tile, mask, asset]));
});

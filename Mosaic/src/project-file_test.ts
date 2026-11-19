import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset, type PropAsset } from "./document.ts";
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
    assert(text.includes('\n  "version": 14,'));
    assert(!text.includes("image bytes"));
    const data = JSON.parse(text);
    assert.deepEqual(Object.keys(data), ["format", "version", "gridSize", "tileSheets", "maskSheets", "maskMode", "propAssets", "props", "layers", "editor"]);
    assert.deepEqual(data.tileSheets, [tile.id]);
    assert.deepEqual(data.maskSheets, [mask.id]);
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
        (data) => { data.editor = null; },
        (data) => { data.maskSheets.push(mask.id); },
        (data) => { data.tileSheets.push(tile.id); },
        (data) => { data.tileSheets = [mask.id]; },
        (data) => { data.maskSheets = [tile.id]; },
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
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    const extra = { ...asset, id: "assets/props/extra.png", label: "extra.png" };
    const project = fixture();
    project.propAssets = [asset];
    project.props = [{ id: 5, asset, x: 2.5, y: 3.25, rotation: 45 },
        { id: 9, asset, x: 1, y: 2, rotation: 270 }];
    project.nextPropId = 10;
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert.deepEqual(data.props, [
        { index: 0, x: 2.5, y: 3.25, rot: 45 },
        { index: 0, x: 1, y: 2, rot: 270 }
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
        (d: typeof data) => { d.props[0].x = "200"; },
        (d: typeof data) => { d.props[0].y = null; },
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
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    const extra = { ...asset, id: "assets/props/new.png", label: "new.png" };
    const project = fixture();
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 1, y: 2, rotation: 90 }];
    project.nextPropId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    for(const fields of [{}, { pivotX: 0 }, { pivotY: 1 }, { scale: 0.25 }, { pivotX: 0.25, pivotY: 1, scale: 2 }])
    {
        data.propAssets = [{ texPath: asset.id, ...fields }];
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, extra, asset]);
        const expected = { pivotX: 0.5, pivotY: 0.5, scale: 1, ...fields };
        assert.deepEqual(loaded.propAssets[0], { ...asset, ...expected });
        assert.equal(loaded.propAssets[1], extra);
        assert.equal(loaded.props[0].asset, loaded.propAssets[0]);
        assert.equal(loaded.props[0].asset.image, asset.image);
        assert.deepEqual([asset.pivotX, asset.pivotY, asset.scale], [0.5, 0.5, 1]);
        const saved = await writeProject(loaded);
        assert.deepEqual(JSON.parse(await saved.text()).propAssets, [
            { texPath: asset.id, ...expected }, { texPath: extra.id, pivotX: 0.5, pivotY: 0.5, scale: 1 }
        ]);
        const again = await readProject(saved, [tile, mask, extra, asset]);
        assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), JSON.parse(await saved.text()));
    }
    for(const entry of [asset.id, {}, null, { texPath: tile.id }, { texPath: 42 },
        ...["pivotX", "pivotY"].flatMap((key) => [-0.01, 1.01, null, "0.5", true].map((value) => ({ texPath: asset.id, [key]: value }))),
        ...[0, -1, null, "1", false].map((scale) => ({ texPath: asset.id, scale }))])
    {
        data.propAssets = [entry];
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
    // Distinct metadata objects must not allow duplicate texture identities.
    data.propAssets = [{ texPath: asset.id }, { texPath: asset.id, scale: 2 }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    data.propAssets = [{ texPath: asset.id, scale: "overflow" }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data).replace('"overflow"', "1e400")]), [tile, mask, asset]));
});

Deno.test("nested paths round-trip and newly discovered catalogs append without changing references", async () => {
    const first = image("tile", "woods/ground.png");
    const second = image("tile", "fields/ground.png");
    const extra = image("tile", "a/ground.png");
    const prop: PropAsset = { kind: "prop", id: "assets/props/old #/stone%20.png", label: "old #/stone%20.png",
        blob: new Blob(), url: "", width: 20, height: 40, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
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
    assert.deepEqual(saved.tileSheets, [first.id, second.id, extra.id]);
    assert.deepEqual(saved.propAssets, [prop, newProp].map((asset) => ({
        texPath: asset.id, pivotX: 0.5, pivotY: 0.5, scale: 1
    })));
    const again = await readProject(await writeProject(loaded), [extra, second, first, mask, newProp, prop]);
    assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), saved);
    await assert.rejects(() => readProject(blob, [first, second, mask, newProp]), /Missing image/);
});

Deno.test("prop coordinates round on load and save without accepting invalid bounds", async () => {
    const project = fixture();
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone",
        blob: new Blob(), url: "", width: 20, height: 20, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 12.12349, y: 31.9998, rotation: 45 }];
    project.nextPropId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    assert.deepEqual(data.props, [{ index: 0, x: 12.123, y: 32, rot: 45 }]);
    assert.equal(project.props[0].x, 12.12349);
    data.props[0].x = 0.0004;
    data.props[0].y = 23.56789;
    const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
    assert.equal(loaded.props[0].x, 0);
    assert.equal(loaded.props[0].y, 23.568);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()).props,
        [{ index: 0, x: 0, y: 23.568, rot: 45 }]);
    for(const invalid of [-32.0001, 32.0001])
    {
        data.props[0].x = invalid;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
});

Deno.test("variable map sizes round-trip and enforce size-specific sparse indices and prop bounds", async () => {
    const asset: PropAsset = { id: "assets/props/a.png", kind: "prop", label: "a",
        blob: new Blob(), url: "", width: 10, height: 10, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
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
        assert.equal(data.version, 14);
        assert.deepEqual(data.layers[1].cells, [size * size - 1]);
        const loaded = await readProject(blob, [tile, mask, asset]);
        assert.equal(loaded.gridSize, size);
        assert.deepEqual(loaded.layers, project.layers);
        assert.deepEqual(loaded.props, project.props);
        assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), data);
        for(const corrupt of [
            (value: typeof data) => { value.layers[1].cells = [size * size]; },
            (value: typeof data) => { value.props[0].x = -size / 2 - 0.0001; },
            (value: typeof data) => { value.props[0].y = size / 2 + 0.0001; },
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

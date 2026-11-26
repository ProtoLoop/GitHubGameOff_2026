import { strict as assert } from "node:assert";
import { createDocument, cloneDocument, sequenceName, sequenceTint, sameContent, type ImageAsset, type SpriteSequence } from "./document.ts";
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
    assert(text.includes('\n  "version": 28,'));
    assert(!text.includes("image bytes"));
    const data = JSON.parse(text);
    assert.deepEqual(Object.keys(data), ["format", "version", "gridSize", "tileSheets", "maskSheets", "maskMode", "sequences", "sprites", "propTypes", "actorTypes", "props", "actors", "layers", "editor"]);
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

Deno.test("extensions preserve JSON values across edits, snapshots, and repeated saves", async () => {
    const original = JSON.parse(await (await writeProject(fixture())).text());
    for(const extensions of [
        { game: { spawn: [1, 2.5], enabled: true, name: "Oak", optional: null }, empty: {}, list: [] },
        {}, [], null, false, true, 0, 1.25, "", "custom metadata"
    ])
    {
        let loaded = await readProject(new Blob([JSON.stringify({ ...original, extensions })]), [tile, mask]);
        loaded.layers[1].painted[101] = true;
        for(let cycle = 0; cycle < 2; cycle++)
        {
            const saved = await writeProject(cloneDocument(loaded));
            const data = JSON.parse(await saved.text());
            assert(Object.hasOwn(data, "extensions"));
            assert.deepEqual(data.extensions, extensions);
            assert(data.layers[1].cells.includes(101));
            loaded = await readProject(saved, [tile, mask]);
        }
    }
    // A different project must not inherit the previous project's extension data.
    const without = await readProject(new Blob([JSON.stringify(original)]), [tile, mask]);
    assert(!Object.hasOwn(JSON.parse(await (await writeProject(without)).text()), "extensions"));
});

Deno.test("level JSON larger than 16 MiB loads and can be saved again", async () => {
    const original = await writeProject(fixture());
    const large = new Blob([" ".repeat(16 * 1024 * 1024), original]);
    assert(large.size > 16 * 1024 * 1024);
    const loaded = await readProject(large, [tile, mask]);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), JSON.parse(await original.text()));
});

Deno.test("file asset paths require a project-root slash while discovered IDs remain relative", async () => {
    const prop: SpriteSequence = { kind: "prop", id: "assets/props/trees #/tree%20.png", label: "trees #/tree%20.png",
        blob: new Blob(), url: "blob:prop", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 1, scale: 0.25, collisionRects: [] };
    const project = fixture();
    project.sequences = [prop];
    project.sprites = [{ name: sequenceName(prop), sequences: [prop] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert.deepEqual(data.tileSheets, ["/" + tile.id]);
    assert.deepEqual(data.maskSheets, ["/" + mask.id]);
    assert.equal(data.sequences[0].texPath, "/" + prop.id);
    const available = [tile, mask, prop];
    const loaded = await readProject(blob, available);
    assert.equal(loaded.tileSheets[0], tile);
    assert.equal(loaded.masks[0], mask);
    assert.equal(loaded.sequences[0].id, prop.id);
    assert.equal(loaded.sequences[0].url, prop.url);
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
            else { invalid.sequences[0].texPath = path; }
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
        (data) => { data.version = 21; },
        (data) => { data.version = 22; },
        (data) => { data.version = 23; },
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
    const asset: SpriteSequence = { id: "assets/props/stone #1.png", kind: "prop", label: "stone #1.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const extra = { ...asset, id: "assets/props/extra.png", label: "extra.png" };
    const project = fixture();
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 5, definition: project.propTypes[0], x: 2.5, y: 3.25, rotation: 45 },
        { id: 9, definition: project.propTypes[0], x: 1, y: 2, rotation: 270 }];
    project.nextPlacementId = 10;
    const blob = await writeProject(project);
    const data = JSON.parse(await blob.text());
    assert.deepEqual(data.props, [
        { type: sequenceName(asset), pos: { x: 2.5, y: 3.25 }, rot: 45 },
        { type: sequenceName(asset), pos: { x: 1, y: 2 }, rot: 270 }
    ]);
    const loaded = await readProject(blob, [tile, mask, extra, asset]);
    assert.deepEqual(loaded.sequences, [{ ...asset, name: sequenceName(asset), size: undefined, tint: sequenceTint(asset) }, extra]);
    assert.deepEqual(loaded.placements.map(({ x, y, rotation }) => ({ x, y, rotation })),
        project.placements.map(({ x, y, rotation }) => ({ x, y, rotation })));
    assert.equal(loaded.nextPlacementId, 2);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()).props, data.props);
    await assert.rejects(() => readProject(blob, [tile, mask]));
    for(const corrupt of [
        (d: typeof data) => { d.version = 7; },
        (d: typeof data) => { delete d.props; },
        (d: typeof data) => { d.props[0].index = 0; delete d.props[0].type; },
        (d: typeof data) => { d.props[0].rotation = d.props[0].rot; delete d.props[0].rot; },
        (d: typeof data) => { d.props[0].type = "missing"; },
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
        (d: typeof data) => { d.sequences = [tile.id]; }
    ])
    {
        const invalid = structuredClone(data);
        corrupt(invalid);
        await assert.rejects(() => readProject(new Blob([JSON.stringify(invalid)]), [tile, mask, asset]));
    }
});

Deno.test("prop metadata defaults independently and round-trips without mutating decoded assets", async () => {
    const asset: SpriteSequence = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const extra = { ...asset, id: "assets/props/new.png", label: "new.png" };
    const project = fixture();
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 0, definition: project.propTypes[0], x: 1, y: 2, rotation: 90 }];
    project.nextPlacementId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    for(const fields of [{}, { pivotX: 0 }, { pivotY: 1 }, { scale: 0.25 }, { pivotX: 0.25, pivotY: 1, scale: 2 }])
    {
        data.sequences = [{ texPath: "/" + asset.id, pivot: { x: fields.pivotX, y: fields.pivotY }, scale: fields.scale }];
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, extra, asset]);
        const expected = { pivotX: 0.5, pivotY: 0.5, scale: 1, ...fields, collisionRects: [] };
        assert.deepEqual(loaded.sequences[0], { ...asset, name: sequenceName(asset), size: undefined, tint: sequenceTint(asset), ...expected });
        assert.equal(loaded.sequences[1], extra);
        assert.equal(loaded.placements[0].definition.sprite.sequences[0], loaded.sequences[0]);
        assert.equal(loaded.placements[0].definition.sprite.sequences[0].image, asset.image);
        assert.deepEqual([asset.pivotX, asset.pivotY, asset.scale], [0.5, 0.5, 1]);
        const saved = await writeProject(loaded);
        assert.deepEqual(JSON.parse(await saved.text()).sequences, [
            { name: sequenceName(asset), tint: sequenceTint(asset), texPath: "/" + asset.id, pivot: { x: expected.pivotX, y: expected.pivotY }, scale: expected.scale, srcRect: {}, collisionRects: [] },
            { name: sequenceName(extra), tint: sequenceTint(extra), texPath: "/" + extra.id, pivot: { x: 0.5, y: 0.5 }, scale: 1, srcRect: {}, collisionRects: [] }
        ]);
        const again = await readProject(saved, [tile, mask, extra, asset]);
        assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), JSON.parse(await saved.text()));
    }
    for(const entry of [asset.id, {}, null, { texPath: "/" + tile.id }, { texPath: 42 },
        ...["x", "y"].flatMap((key) => [-0.01, 1.01, null, "0.5", true].map((value) => ({ texPath: "/" + asset.id, pivot: { [key]: value } }))),
        ...[0, -1, null, "1", false].map((scale) => ({ texPath: "/" + asset.id, scale }))])
    {
        data.sequences = [entry];
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
    // Distinct metadata objects must not allow duplicate sequence names.
    data.sequences = [{ texPath: "/" + asset.id }, { texPath: "/" + asset.id, scale: 2 }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    data.sequences = [{ texPath: "/" + asset.id, scale: "overflow" }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data).replace('"overflow"', "1e400")]), [tile, mask, asset]));
});

Deno.test("nested paths round-trip and newly discovered catalogs append without changing references", async () => {
    const first = image("tile", "woods/ground.png");
    const second = image("tile", "fields/ground.png");
    const extra = image("tile", "a/ground.png");
    const prop: SpriteSequence = { kind: "prop", id: "assets/props/old #/stone%20.png", label: "old #/stone%20.png",
        blob: new Blob(), url: "", width: 20, height: 40, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 20, h: 40 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const newProp = { ...prop, name: "newStone", id: "assets/props/new/stone%20.png", label: "new/stone%20.png" };
    const project = createDocument([first, second], [mask]);
    project.sequences = [prop];
    project.sprites = [{ name: sequenceName(prop), sequences: [prop] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 0, definition: project.propTypes[0], x: 1, y: 2, rotation: 0 }];
    project.nextPlacementId = 1;
    const blob = await writeProject(project);
    const loaded = await readProject(blob, [extra, second, first, mask, newProp, prop]);
    assert.deepEqual(loaded.tileSheets, [first, second, extra]);
    assert.deepEqual(loaded.sequences, [{ ...prop, name: sequenceName(prop), size: undefined, tint: sequenceTint(prop) }, newProp]);
    assert.equal(loaded.layers[1].sheet, second);
    assert.equal(loaded.placements[0].definition.sprite.sequences[0], loaded.sequences[0]);
    assert.equal(loaded.placements[0].definition.sprite.sequences[0].image, prop.image);
    const saved = JSON.parse(await (await writeProject(loaded)).text());
    assert.deepEqual(saved.tileSheets, [first, second, extra].map((asset) => "/" + asset.id));
    assert.deepEqual(saved.sequences, [prop, newProp].map((asset) => ({
        name: sequenceName(asset), tint: sequenceTint(asset), texPath: "/" + asset.id, pivot: { x: 0.5, y: 0.5 }, scale: 1, srcRect: {}, collisionRects: []
    })));
    const again = await readProject(await writeProject(loaded), [extra, second, first, mask, newProp, prop]);
    assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), saved);
    await assert.rejects(() => readProject(blob, [first, second, mask, newProp]), /Missing image/);
});

Deno.test("prop coordinates round on load and save without accepting invalid bounds", async () => {
    const project = fixture();
    const asset: SpriteSequence = { id: "assets/props/stone.png", kind: "prop", label: "stone",
        blob: new Blob(), url: "", width: 20, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 20, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 0, definition: project.propTypes[0], x: 12.12349, y: 31.9998, rotation: 45 }];
    project.nextPlacementId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    assert.deepEqual(data.props, [{ type: sequenceName(asset), pos: { x: 12.123, y: 32 }, rot: 45 }]);
    assert.equal(project.placements[0].x, 12.12349);
    data.props[0].pos.x = 0.0004;
    data.props[0].pos.y = 23.56789;
    const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
    assert.equal(loaded.placements[0].x, 0);
    assert.equal(loaded.placements[0].y, 23.568);
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()).props,
        [{ type: sequenceName(asset), pos: { x: 0, y: 23.568 }, rot: 45 }]);
    for(const invalid of [-32.0001, 32.0001])
    {
        data.props[0].pos.x = invalid;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
});

Deno.test("variable map sizes round-trip and enforce size-specific sparse indices and prop bounds", async () => {
    const asset: SpriteSequence = { id: "assets/props/a.png", kind: "prop", label: "a",
        blob: new Blob(), url: "", width: 10, height: 10, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 10, h: 10 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    for(const size of [2, 4, 8, 16, 32, 64, 128, 256, 512])
    {
        const project = fixture();
        project.gridSize = size;
        project.layers = project.layers.map((layer, index) => ({ ...layer, painted: Array(size * size).fill(index === 0) }));
        project.layers[1].painted[size * size - 1] = true;
        project.sequences = [asset];
        project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
        project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
        project.placements = [{ id: 0, definition: project.propTypes[0], x: -size / 2, y: size / 2, rotation: 0 }];
        project.nextPlacementId = 1;
        const blob = await writeProject(project);
        const data = JSON.parse(await blob.text());
        assert.equal(data.version, 28);
        assert.deepEqual(data.layers[1].cells, [size * size - 1]);
        const loaded = await readProject(blob, [tile, mask, asset]);
        assert.equal(loaded.gridSize, size);
        assert.deepEqual(loaded.layers, project.layers);
        assert.deepEqual(loaded.placements, project.placements.map((prop) => ({ ...prop, definition: loaded.propTypes[0] })));
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
    const asset: SpriteSequence = { id: "assets/props/stone.png", kind: "prop", label: "stone",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const project = fixture();
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    const data = JSON.parse(await (await writeProject(project)).text());
    const collisionRects = [{ x: -0.2, y: 0.75, w: 1.5, h: 0.2 }, { x: 0.3, y: 0.8, w: 0.4, h: 0.5 }];
    for(const value of [undefined, [], collisionRects])
    {
        data.sequences[0].collisionRects = value;
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
        assert.deepEqual(loaded.sequences[0].collisionRects, value ?? []);
        assert.deepEqual(asset.collisionRects, []);
        const saved = JSON.parse(await (await writeProject(loaded)).text());
        assert.deepEqual(saved.sequences[0].collisionRects, value ?? []);
        assert(!("bounds" in saved.sequences[0]));
        assert(!("collider" in saved.sequences[0]));
        assert.deepEqual(saved.sequences[0].pivot, { x: 0.5, y: 0.5 });
        assert(!("pivotX" in saved.sequences[0]));
    }
    for(const value of [null, {}, 1, [null], [[]], [{}],
        ...["x", "y", "w", "h"].flatMap((key) => [undefined, null, "1", true].map((bad) => [{ ...collisionRects[0], [key]: bad }])),
        ...["w", "h"].flatMap((key) => [0, -1].map((bad) => [{ ...collisionRects[0], [key]: bad }]))])
    {
        data.sequences[0].collisionRects = value;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
    for(const key of ["x", "y", "w", "h"])
    {
        for(const bad of [NaN, Infinity, -Infinity])
        {
            project.sequences = [{ ...asset, collisionRects: [{ ...collisionRects[0], [key]: bad }] }];
            assert.throws(() => writeProject(project), /collision rectangles/);
        }
    }
    data.sequences[0].collisionRects = [];
    for(const pivot of [null, [], 1, "center"])
    {
        data.sequences[0].pivot = pivot;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
});

Deno.test("source rectangles default to the full texture and round-trip without changing decoded assets", async () => {
    const asset: SpriteSequence = { kind: "prop", id: "assets/props/atlas.png", label: "atlas",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 }, pivotX: 0.25, pivotY: 1, scale: 2, collisionRects: [] };
    const project = fixture();
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 0, definition: project.propTypes[0], x: 1, y: 2, rotation: 90 }];
    project.nextPlacementId = 1;
    const before = cloneDocument(project);
    const data = JSON.parse(await (await writeProject(project)).text());
    for(const srcRect of [undefined, {}, { x: 0, y: 0, w: 80, h: 20 },
        { x: 0, y: 0, w: 79, h: 20 }, { x: 0, y: 0, w: 80, h: 19 },
        { x: 32, y: 4, w: 48, h: 16 }, { x: 79, y: 19, w: 1, h: 1 }])
    {
        data.sequences[0].srcRect = srcRect;
        const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
        const fullTexture = srcRect === undefined || Object.keys(srcRect).length === 0
            || (srcRect.x === 0 && srcRect.y === 0 && srcRect.w === 80 && srcRect.h === 20);
        assert.deepEqual(loaded.sequences[0].srcRect, fullTexture ? asset.srcRect : srcRect);
        assert.equal(loaded.placements[0].definition.sprite.sequences[0], loaded.sequences[0]);
        assert.equal(loaded.placements[0].definition.sprite.sequences[0].image, asset.image);
        assert.deepEqual(loaded.sequences[0].collisionRects, asset.collisionRects);
        assert.equal(loaded.sequences[0].pivotX, asset.pivotX);
        const saved = await writeProject(loaded);
        assert.deepEqual(JSON.parse(await saved.text()).sequences[0].srcRect, fullTexture ? {} : srcRect);
        const again = await readProject(saved, [tile, mask, asset]);
        assert.deepEqual(again.sequences[0], loaded.sequences[0]);
        assert.equal(cloneDocument(loaded).sequences[0], loaded.sequences[0]);
    }
    assert(sameContent(project, before));
    assert.deepEqual(asset.srcRect, { x: 0, y: 0, w: 80, h: 20 });
});

Deno.test("source rectangles reject malformed, fractional and out-of-texture regions on load and save", async () => {
    const asset: SpriteSequence = { kind: "prop", id: "assets/props/atlas.png", label: "atlas",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 }, pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const project = fixture();
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    const data = JSON.parse(await (await writeProject(project)).text());
    const rect = { x: 5, y: 2, w: 10, h: 8 };
    const invalid = [null, [], [0, 0, 80, 20], [{}], { unknown: true }, "rectangle", ...["x", "y", "w", "h"].flatMap((key) =>
        [undefined, null, "1", true, -1, 0.5, NaN, Infinity, Number.MAX_SAFE_INTEGER].map((value) => ({ ...rect, [key]: value }))),
        { ...rect, w: 0 }, { ...rect, h: 0 }, { ...rect, x: 71 }, { ...rect, y: 13 }];
    for(const srcRect of invalid)
    {
        data.sequences[0].srcRect = srcRect;
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
        project.sequences = [{ ...asset, srcRect: srcRect as SpriteSequence["srcRect"] }];
        assert.throws(() => writeProject(project), /source rectangle/);
    }
    // Empty objects are file shorthand only. In-memory rectangles must be resolved.
    project.sequences = [{ ...asset, srcRect: {} as SpriteSequence["srcRect"] }];
    assert.throws(() => writeProject(project), /source rectangle/);
    // JSON overflow must fail as well, independently of JSON.stringify converting Infinity to null.
    data.sequences[0].srcRect = { ...rect, x: "overflow" };
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data).replace('"overflow"', "1e400")]), [tile, mask, asset]));
});

Deno.test("named sequences share textures, preserve optional frame dimensions and tint across saves", async () => {
    const asset: SpriteSequence = { kind: "prop", id: "assets/anims/actor.png", label: "actor",
        blob: new Blob(), url: "blob:actor", width: 128, height: 64, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 128, h: 64 }, pivotX: 0.5, pivotY: 1, scale: 1, collisionRects: [] };
    const project = fixture();
    const data = JSON.parse(await (await writeProject(project)).text());
    const entries = [
        { name: "Walk", texPath: "/" + asset.id, srcRect: { x: 16, y: 8, w: 100, h: 40 },
            size: { w: 24, h: 16 }, tint: { r: 2, g: 0, b: 3, a: 1.5 } },
        { name: "Portrait", texPath: "/" + asset.id, srcRect: { x: 16, y: 8, w: 32, h: 40 } }
    ];
    data.sequences = entries;
    data.sprites = [{ name: "Actor", sequences: ["Portrait", "Walk"] }];
    data.propTypes = [{ name: "Actor", sprite: "Actor" }];
    data.props = [{ type: "Actor", pos: { x: 0, y: 0 }, rot: 0 }];
    data.extensions = { animations: ["Walk"] };
    let loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]);
    assert.equal(loaded.sequences.length, 2);
    assert.equal(loaded.sequences[0].image, loaded.sequences[1].image);
    assert.equal(loaded.sequences[0].url, asset.url);
    assert.equal(loaded.sequences[1].size, undefined);
    for(let cycle = 0; cycle < 2; cycle++)
    {
        loaded.layers[1].painted[101] = true;
        const saved = await writeProject(cloneDocument(loaded));
        const output = JSON.parse(await saved.text());
        assert.deepEqual(output.sequences.map((entry: { name: string }) => entry.name), ["Walk", "Portrait"]);
        assert.deepEqual(output.sequences[0].size, entries[0].size);
        assert(!Object.hasOwn(output.sequences[0], "dim"));
        assert.deepEqual(output.sequences[0].tint, entries[0].tint);
        assert(!Object.hasOwn(output.sequences[1], "size"));
        assert.deepEqual(output.sequences[1].tint, { r: 1, g: 1, b: 1, a: 1 });
        assert.equal(output.props[0].type, "Actor");
        assert.deepEqual(output.extensions, data.extensions);
        loaded = await readProject(saved, [tile, mask, asset]);
    }
});

Deno.test("sequence metadata rejects invalid names, frame dimensions and tints", async () => {
    const asset: SpriteSequence = { kind: "prop", id: "assets/actor.png", label: "actor",
        blob: new Blob(), url: "", width: 128, height: 64, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 128, h: 64 }, pivotX: 0.5, pivotY: 1, scale: 1, collisionRects: [] };
    const data = JSON.parse(await (await writeProject(fixture())).text());
    const entry = { name: "Walk", texPath: "/" + asset.id, srcRect: { x: 0, y: 0, w: 32, h: 32 } };
    for(const fields of [
        ...[null, "", " ", 42, "a".repeat(64), "é".repeat(32), "bad\u0000name"].map((name) => ({ name })),
        ...[null, {}, [], { w: 8 }, { w: 0, h: 8 }, { w: 33, h: 8 }, { w: 8, h: 33 },
            { w: 1.5, h: 8 }].map((size) => ({ size })),
        ...[null, {}, [], { r: 1, g: 1, b: 1 }, { r: -0.1, g: 1, b: 1, a: 1 },
            { r: 1, g: 1, b: 1, a: -0.1 }].map((tint) => ({ tint }))
    ])
    {
        data.sequences = [{ ...entry, ...fields }];
        await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
    }
    for(const key of ["r", "g", "b", "a"])
    {
        data.sequences = [{ ...entry, tint: { r: 1, g: 1, b: 1, a: 1, [key]: "overflow" } }];
        await assert.rejects(() => readProject(
            new Blob([JSON.stringify(data).replace('"overflow"', "1e400")]), [tile, mask, asset]), /tint/);
    }
    data.sequences = [entry, { ...entry, size: { w: 8, h: 8 } }];
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, asset]));
});

Deno.test("sprite names resolve independently of catalog order and discovery preserves explicit groups", async () => {
    const north: SpriteSequence = { kind: "prop", id: "assets/props/actor.png", label: "actor", name: "North",
        blob: new Blob(), url: "blob:actor", image: {} as CanvasImageSource, width: 64, height: 32,
        srcRect: { x: 0, y: 0, w: 64, h: 16 }, size: { w: 16, h: 16 },
        pivotX: 0.5, pivotY: 1, scale: 1, collisionRects: [] };
    const south = { ...north, name: "South", srcRect: { x: 0, y: 16, w: 64, h: 16 } };
    const extra = { ...north, name: "Tree", id: "assets/props/tree.png" };
    const project = fixture();
    project.sequences = [north, south];
    project.sprites = [{ name: "Actor", sequences: [south, north] }, { name: "Portrait", sequences: [north] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 0, definition: project.propTypes[0], x: 1, y: 2, rotation: 90 }];
    project.nextPlacementId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    data.sequences.reverse();
    data.sprites.reverse();
    const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, north, extra]);
    assert.equal(loaded.placements[0].definition.sprite, loaded.sprites[1]);
    assert.deepEqual(loaded.placements[0].definition.sprite.sequences.map(sequenceName), ["South", "North"]);
    assert.equal(loaded.placements[0].definition.sprite.sequences[0], loaded.sequences[0]);
    assert.equal(loaded.sprites[0].sequences[0], loaded.placements[0].definition.sprite.sequences[1]);
    assert.deepEqual(loaded.sprites.map((sprite) => sprite.name), ["Portrait", "Actor", "Tree"]);
    assert.equal(loaded.sprites[2].sequences[0], extra);
    const saved = await writeProject(loaded);
    const output = JSON.parse(await saved.text());
    assert.deepEqual(output.sprites, [...data.sprites, { name: "Tree", sequences: ["Tree"] }]);
    assert.deepEqual(output.props, data.props);
    assert(!Object.hasOwn(output, "propAssets"));
    assert(!Object.hasOwn(output.props[0], "index"));
    const again = await readProject(saved, [tile, mask, north, extra]);
    assert.deepEqual(JSON.parse(await (await writeProject(again)).text()), output);
    // A newly discovered sequence must not silently join or rename an existing sprite.
    const conflicting = { ...extra, name: "Actor" };
    await assert.rejects(() => readProject(new Blob([JSON.stringify(data)]), [tile, mask, north, conflicting]), /duplicate sprite name/);
});

Deno.test("sprite catalogs reject malformed names, empty groups, missing references and old versions", async () => {
    const sequence: SpriteSequence = { kind: "prop", id: "assets/props/stone.png", label: "stone", name: "Stone",
        blob: new Blob(), url: "", image: {} as CanvasImageSource, width: 16, height: 16,
        srcRect: { x: 0, y: 0, w: 16, h: 16 }, pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    const project = fixture();
    project.sequences = [sequence];
    project.sprites = [{ name: "Stone", sequences: [sequence] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    project.placements = [{ id: 0, definition: project.propTypes[0], x: 0, y: 0, rotation: 0 }];
    project.nextPlacementId = 1;
    const data = JSON.parse(await (await writeProject(project)).text());
    for(const corrupt of [
        (d: typeof data) => { d.version = 24; },
        (d: typeof data) => { d.propAssets = d.sequences; delete d.sequences; },
        (d: typeof data) => { delete d.sprites; },
        (d: typeof data) => { d.sprites = null; },
        (d: typeof data) => { d.sprites = [null]; },
        (d: typeof data) => { d.sprites.push({ ...d.sprites[0] }); },
        (d: typeof data) => { d.sprites = Array(257).fill(d.sprites[0]); },
        ...[undefined, null, 1, "", " ", "a".repeat(64), "é".repeat(32), "bad\nname"].map((name) =>
            (d: typeof data) => { d.sprites[0].name = name; }),
        ...[undefined, null, {}, [], ["Missing"], [0], [null], Array(257).fill("Stone")].map((sequences) =>
            (d: typeof data) => { d.sprites[0].sequences = sequences; }),
        ...[undefined, null, 0, "Missing"].map((sprite) =>
            (d: typeof data) => { d.props[0].type = sprite; })
    ])
    {
        const invalid = structuredClone(data);
        corrupt(invalid);
        await assert.rejects(() => readProject(new Blob([JSON.stringify(invalid)]), [tile, mask, sequence]));
    }
});

Deno.test("explicit object catalogs round-trip by name without promoting discovered sprites", async () => {
    const image: SpriteSequence = { kind: "prop", id: "assets/props/stone.png", label: "stone",
        blob: new Blob(), url: "", width: 20, height: 40, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 20, h: 40 }, pivotX: 0.5, pivotY: 1, scale: 1, collisionRects: [] };
    const project = fixture();
    project.sequences = [image];
    project.sprites = [{ name: "Stone", sequences: [image] }];
    project.propTypes = [{ kind: "prop", name: "Decoration", sprite: project.sprites[0] }];
    project.actorTypes = [
        { kind: "actor", name: "Grave", class: "Tombstone", sprite: project.sprites[0] },
        { kind: "actor", name: "Other Grave", class: "Tombstone", sprite: project.sprites[0] }
    ];
    project.placements = [
        { id: 4, definition: project.propTypes[0], x: 1, y: 2, rotation: 0 },
        { id: 7, definition: project.actorTypes[0], x: -1.23456, y: 3, rotation: 45 },
        { id: 8, definition: project.actorTypes[1], x: 2, y: -3, rotation: 90 }
    ];
    project.nextPlacementId = 9;
    const data = JSON.parse(await (await writeProject(project)).text());
    assert.deepEqual(data.props, [{ type: "Decoration", pos: { x: 1, y: 2 }, rot: 0 }]);
    assert.deepEqual(data.actors, [
        { type: "Grave", pos: { x: -1.235, y: 3 }, rot: 45 },
        { type: "Other Grave", pos: { x: 2, y: -3 }, rot: 90 }
    ]);
    assert.deepEqual(data.actorTypes[0], { name: "Grave", class: "Tombstone", sprite: "Stone" });
    assert(!Object.hasOwn(data.props[0], "prop"));
    assert(!Object.hasOwn(data.actors[0], "actor"));
    assert(!Object.hasOwn(data.actorTypes[0], "type"));
    data.actorTypes.reverse();
    const extra = { ...image, id: "assets/props/discovered.png" };
    const loaded = await readProject(new Blob([JSON.stringify(data)]), [tile, mask, image, extra]);
    assert.equal(loaded.placements[1].definition, loaded.actorTypes[1]);
    assert.equal(loaded.placements[2].definition, loaded.actorTypes[0]);
    assert.equal(loaded.sprites.length, 2);
    assert.equal(loaded.propTypes.length, 1);
    assert.equal(loaded.actorTypes.length, 2);
    const saved = JSON.parse(await (await writeProject(loaded)).text());
    assert.deepEqual(saved.actorTypes, data.actorTypes);
    assert.deepEqual(saved.actors, data.actors);
    assert.deepEqual(saved.props, data.props);
    assert(!Object.hasOwn(saved.actors[0], "id"));

    for(const corrupt of [
        (d: typeof data) => { d.version = 25; },
        (d: typeof data) => { d.version = 26; },
        (d: typeof data) => { d.version = 27; },
        (d: typeof data) => { d.props[0].prop = d.props[0].type; delete d.props[0].type; },
        (d: typeof data) => { d.actors[0].actor = d.actors[0].type; delete d.actors[0].type; },
        (d: typeof data) => { d.actorTypes[0].type = d.actorTypes[0].class; delete d.actorTypes[0].class; },
        (d: typeof data) => { delete d.actorTypes; },
        (d: typeof data) => { d.propTypes = null; },
        (d: typeof data) => { d.actorTypes.push({ ...d.actorTypes[0] }); },
        (d: typeof data) => { d.propTypes.push({ ...d.propTypes[0] }); },
        (d: typeof data) => { d.actorTypes[0].sprite = "missing"; },
        (d: typeof data) => { d.propTypes[0].sprite = "missing"; },
        (d: typeof data) => { d.actorTypes[0].class = ""; },
        (d: typeof data) => { d.actorTypes[0].class = null; },
        (d: typeof data) => { d.actorTypes[0].name = " "; },
        (d: typeof data) => { d.actorTypes[0].name = "x".repeat(64); },
        (d: typeof data) => { d.actors[0].type = "Decoration"; },
        (d: typeof data) => { d.props[0].type = "Grave"; },
        (d: typeof data) => { d.actors[0].sprite = "Stone"; delete d.actors[0].type; },
        (d: typeof data) => { d.actors[0].pos.x = 32.0001; },
        (d: typeof data) => { d.actors[0].rot = 360; },
        (d: typeof data) => { d.actors = Array(10000).fill(d.actors[0]); }
    ])
    {
        const invalid = structuredClone(data);
        corrupt(invalid);
        await assert.rejects(() => readProject(new Blob([JSON.stringify(invalid)]), [tile, mask, image]));
    }
});

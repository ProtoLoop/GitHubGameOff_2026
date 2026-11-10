import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset } from "./document.ts";
import { readProject, writeProject } from "./project-file.ts";
import { automaticTile, cellMasks } from "./placement.ts";

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
    assert(text.includes('\n  "version": 4,'));
    assert(!text.includes("image bytes"));
    const data = JSON.parse(text);
    assert.deepEqual(Object.keys(data), ["format", "version", "gridSize", "tileSize", "tileSheets", "maskSheets", "maskMode", "layers", "editor"]);
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
        assert.deepEqual(automaticTile(loaded, loaded.layers[1], cell), automaticTile(project, project.layers[1], cell));
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
        (data) => { data.version = 1; },
        (data) => { data.editor = null; },
        (data) => { data.maskSheets.push(mask.id); },
        (data) => { data.tileSheets.push(tile.id); },
        (data) => { data.tileSheets = [mask.id]; },
        (data) => { data.maskSheets = [tile.id]; },
        (data) => { data.gridSize = 32; },
        (data) => { data.version = 3; },
        (data) => { data.version = 5; },
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
            assert.deepEqual(automaticTile(loaded, loaded.layers[index], cell), automaticTile(project, project.layers[index], cell));
            if(index !== 0)
            {
                assert.deepEqual(cellMasks(loaded, loaded.layers[index], cell), cellMasks(project, project.layers[index], cell));
            }
        }
    }
    assert.deepEqual(JSON.parse(await (await writeProject(loaded)).text()), data);
});

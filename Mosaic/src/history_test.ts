import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset } from "./document.ts";
import { automaticTile, cellMasks, terrainSourceOffset } from "./placement.ts";
import { History } from "./history.ts";

function fixture()
{
    const image = (kind: "tile" | "mask"): ImageAsset => ({ id: `assets/${kind === "tile" ? "tiles" : "masks"}/${kind}.png`, kind, label: `${kind}.png`,
        blob: new Blob([kind]), dimension: 256, pixels: new Uint8ClampedArray(0),
        coverage: new Uint8Array(0), url: `blob:${kind}` });
    return createDocument([image("tile")], [image("mask")]);
}

Deno.test("a complete stroke and higher-layer clearing form one undo action", () => {
    let project = fixture();
    project.layers.push({ ...project.layers[1], id: 2, painted: Array(4096).fill(true) });
    project.nextLayerId = 3;
    const originalMasks = cellMasks(project, project.layers[2], 11);
    const history = new History(100, 64 * 1024 * 1024);
    history.begin(project, "Paint stroke");
    for(const cell of [10, 11, 12])
    {
        project.layers[1].painted[cell] = true;
        project.layers[2].painted[cell] = false;
    }
    assert.equal(history.committedState(project).layers[1].painted[11], false);
    assert.equal(history.commit(project), true);
    project = history.undo(project)!;
    assert.equal(project.layers[1].painted[11], false);
    assert.equal(project.layers[2].painted[11], true);
    assert.deepEqual(cellMasks(project, project.layers[2], 11), originalMasks);
    assert.equal(history.undoLabel, null);
    project = history.redo(project)!;
    assert.equal(project.layers[1].painted[11], true);
    assert.equal(project.layers[2].painted[11], false);
});

Deno.test("history preserves redo for no-ops, drops it for edits, and retains image references", () => {
    let project = fixture();
    const history = new History(2, 64 * 1024 * 1024);
    const originalImage = project.layers[1].sheet;
    history.begin(project, "Remove layer");
    project.layers.pop();
    history.commit(project);
    project = history.undo(project)!;
    assert.equal(project.layers[1].sheet, originalImage);
    history.begin(project, "No change");
    assert.equal(history.commit(project), false);
    assert.equal(history.redoLabel, "Remove layer");
    history.begin(project, "Paint");
    project.layers[1].painted[0] = true;
    history.commit(project);
    assert.equal(history.redoLabel, null);
    for(let i = 0; i < 5; i++)
    {
        history.begin(project, `Edit ${i}`);
        project.layers[1].painted[i + 1] = true;
        history.commit(project);
    }
    assert.equal(history.retainedStates().length, 2);
    const tiny = new History(100, 1);
    tiny.begin(project, "Edit");
    project.layers[1].painted[10] = true;
    tiny.commit(project);
    assert.equal(tiny.undoLabel, null);
});

Deno.test("undo and redo restore the global terrain size and sampling without changing occupancy", () => {
    let project = fixture();
    project.tileSize = 64;
    project.layers[1].painted[65] = true;
    const history = new History(100, 64 * 1024 * 1024);
    const sample = () => terrainSourceOffset(project, project.layers[1], automaticTile(project, project.layers[1], 65), 17, 29);
    const original = sample();
    const masks = cellMasks(project, project.layers[1], 65);
    history.begin(project, "Change terrain tile size");
    project.tileSize = 128;
    assert(history.commit(project));
    const changed = sample();
    assert.notEqual(changed, original);
    project = history.undo(project)!;
    assert.equal(project.tileSize, 64);
    assert.equal(sample(), original);
    assert(project.layers[1].painted[65]);
    project = history.redo(project)!;
    assert.equal(project.tileSize, 128);
    assert.equal(sample(), changed);
    assert(project.layers[1].painted[65]);
    assert.deepEqual(cellMasks(project, project.layers[1], 65), masks);
});

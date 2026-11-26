import { strict as assert } from "node:assert";
import { sequenceName, createDocument, sameTerrainContent, type ImageAsset, type SpriteSequence, type ProjectDocument } from "./document.ts";
import { cellMasks, terrainSourceOffset } from "./placement.ts";
import { History } from "./history.ts";

function fixture()
{
    const image = (kind: "tile" | "mask"): ImageAsset => ({ id: `assets/${kind === "tile" ? "tiles" : "masks"}/${kind}.png`, kind, label: `${kind}.png`,
        blob: new Blob([kind]), dimension: 256, pixels: new Uint8ClampedArray(0),
        coverage: new Uint8Array(0), url: `blob:${kind}` });
    return createDocument([image("tile")], [image("mask")]);
}

Deno.test("extensions survive committed snapshots, undo, redo, and rollback", () => {
    const extensions = { game: { tags: ["forest"], spawn: { x: 2, y: 3 } } };
    let project: ProjectDocument = { ...fixture(), extensions };
    const history = new History(100, 64 * 1024 * 1024);
    history.begin(project, "Paint");
    project.layers[1].painted[0] = true;
    assert.equal(history.committedState(project).extensions, extensions);
    assert(history.commit(project));
    const undone = history.undo(project)!;
    assert.equal(undone.extensions, extensions);
    assert.equal(undone.layers[1].painted[0], false);
    const redone = history.redo(undone)!;
    assert.equal(redone.extensions, extensions);
    assert.equal(redone.layers[1].painted[0], true);
    project = redone;
    history.begin(project, "Cancelled paint");
    project.layers[1].painted[1] = true;
    const restored = history.rollback()!;
    assert.equal(restored.extensions, extensions);
    assert.equal(restored.layers[1].painted[1], false);
});

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

Deno.test("undo and redo restore terrain materials without changing occupancy or masks", () => {
    let project = fixture();
    project.layers[1].painted[65] = true;
    const history = new History(100, 64 * 1024 * 1024);
    const sample = () => terrainSourceOffset(project, project.layers[1], 65, 17, 29);
    const original = project.layers[1].sheet;
    const offset = sample();
    const replacement = { ...original, id: "assets/tiles/replacement.png", label: "replacement.png" };
    project.tileSheets = [...project.tileSheets, replacement];
    const masks = cellMasks(project, project.layers[1], 65);
    history.begin(project, "Change material");
    project.layers[1].sheet = replacement;
    assert(history.commit(project));
    assert.equal(sample(), offset);
    project = history.undo(project)!;
    assert.equal(project.layers[1].sheet, original);
    assert.equal(sample(), offset);
    assert(project.layers[1].painted[65]);
    project = history.redo(project)!;
    assert.equal(project.layers[1].sheet, replacement);
    assert.equal(sample(), offset);
    assert(project.layers[1].painted[65]);
    assert.deepEqual(cellMasks(project, project.layers[1], 65), masks);
});

Deno.test("prop placement, multi-event movement, rotation and deletion undo as complete actions", () => {
    let project = fixture();
    const asset: SpriteSequence = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        name: "Stone", size: { w: 20, h: 20 }, tint: { r: 0.5, g: 1, b: 1, a: 0.7 },
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    const history = new History(100, 64 * 1024 * 1024);
    history.begin(project, "Place prop");
    project.placements.push({ id: project.nextPlacementId++, definition: project.propTypes[0], x: 1, y: 2, rotation: 0 });
    history.commit(project);
    history.begin(project, "Move prop");
    project.placements[0].x = 1.5;
    project.placements[0].x = 2.0;
    assert.equal(history.committedState(project).placements[0].x, 1);
    history.commit(project);
    history.begin(project, "Rotate prop");
    project.placements[0].rotation = 45;
    history.commit(project);
    history.begin(project, "Delete prop");
    project.placements = [];
    history.commit(project);
    project = history.undo(project)!;
    assert.equal(project.placements[0].rotation, 45);
    project = history.undo(project)!;
    assert.equal(project.placements[0].rotation, 0);
    project = history.undo(project)!;
    assert.equal(project.placements[0].x, 1);
    project = history.undo(project)!;
    assert.equal(project.placements.length, 0);
    project = history.redo(project)!;
    assert.equal(project.placements[0].definition.sprite.sequences[0], asset);
    assert.equal(project.placements[0].definition.sprite, project.sprites[0]);
    assert.deepEqual(project.placements[0].definition.sprite.sequences[0].size, { w: 20, h: 20 });
    assert.deepEqual(project.placements[0].definition.sprite.sequences[0].tint, { r: 0.5, g: 1, b: 1, a: 0.7 });
    assert.equal(project.placements[0].definition.sprite.sequences[0].name, "Stone");
    history.begin(project, "Cancelled drag");
    project.placements[0].x = 3.33;
    project = history.rollback()!;
    assert.equal(project.placements[0].x, 1);
    assert.equal(project.placements[0].definition.sprite, project.sprites[0]);
    assert(history.retainedStates().some((state) => state.sequences.includes(asset)));
});

Deno.test("terrain comparison skips prop edits through undo and redo but detects terrain edits and rollback", () => {
    let project = fixture();
    const asset: SpriteSequence = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite }));
    const history = new History(100, 64 * 1024 * 1024);
    for(const mutate of [
        () => { project.placements.push({ id: project.nextPlacementId++, definition: project.propTypes[0], x: 1, y: 2, rotation: 0 }); },
        () => { project.placements[0].x = 3.0; },
        () => { project.placements[0].rotation = 90; },
        () => { project.placements = []; }
    ])
    {
        history.begin(project, "Prop edit");
        const before = history.committedState(project);
        mutate();
        assert(sameTerrainContent(before, project));
        assert(history.commit(project));
        const undone = history.undo(project)!;
        assert(sameTerrainContent(project, undone));
        const redone = history.redo(undone)!;
        assert(sameTerrainContent(undone, redone));
        project = redone;
    }
    history.begin(project, "Paint terrain");
    const beforePaint = history.committedState(project);
    project.layers[1].painted[65] = true;
    assert(!sameTerrainContent(beforePaint, project));
    history.commit(project);
    const undone = history.undo(project)!;
    assert(!sameTerrainContent(project, undone));
    project = history.redo(undone)!;
    assert(!sameTerrainContent(undone, project));
    history.begin(project, "Cancelled stroke");
    project.layers[1].painted[66] = true;
    const restored = history.rollback()!;
    assert(!sameTerrainContent(project, restored));
});

Deno.test("history records both resize directions even when occupancy is otherwise empty", () => {
    let project = fixture();
    project.layers[1].painted.fill(false);
    const history = new History(100, 64 * 1024 * 1024);
    for(const size of [2, 64, 4])
    {
        history.begin(project, "Resize map");
        project.gridSize = size;
        project.layers = project.layers.map((layer, index) => ({ ...layer, painted: Array(size * size).fill(index === 0) }));
        assert(history.commit(project));
    }
    for(const size of [64, 2, 64])
    {
        project = history.undo(project)!;
        assert.equal(project.gridSize, size);
        assert.equal(project.layers[0].painted.length, size * size);
    }
    for(const size of [2, 64, 4])
    {
        project = history.redo(project)!;
        assert.equal(project.gridSize, size);
        assert.equal(project.layers[1].painted.length, size * size);
    }
});

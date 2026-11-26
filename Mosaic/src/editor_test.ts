import { strict as assert } from "node:assert";
import { Editor } from "./editor.ts";
import { scaleHandles } from "./props.ts";
import { History } from "./history.ts";
import { sequenceName, createDocument, cloneDocument, sameContent, validateDocument, type ImageAsset, type SpriteSequence } from "./document.ts";

function fixture(): Editor
{
    const tile: ImageAsset = { kind: "tile", id: "assets/tiles/a.png", label: "a",
        blob: new Blob(), url: "", dimension: 256, pixels: new Uint8ClampedArray(), coverage: new Uint8Array() };
    const other = { ...tile, id: "assets/tiles/b.png" };
    const project = createDocument([tile, other], [{ ...tile, kind: "mask", id: "assets/masks/a.png" }]);
    project.layers.push({ id: 2, sheet: tile, painted: Array(project.gridSize * project.gridSize).fill(false) });
    project.nextLayerId = 3;
    const asset: SpriteSequence = { kind: "prop", id: "assets/props/a.png", label: "a",
        blob: new Blob(), url: "", width: 20, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 20, h: 20 },
        pivotX: 0.5, pivotY: 0.5, collisionRects: [] };
    project.sequences = [asset];
    project.sprites = [{ name: sequenceName(asset), sequences: [asset] }];
    project.propTypes = project.sprites.map((sprite) => ({ kind: "prop", name: sprite.name, sprite, scale: 1 }));
    project.placements = [2, 6, 10].map((x, id) => ({ id, definition: project.propTypes[0], x, y: 2, rotation: 0 }));
    project.nextPlacementId = 3;
    return new Editor(project, new History(100, 64 * 1024 * 1024));
}

function select(editor: Editor, x: number, shift: boolean): void
{
    editor.beginSelection(1, { x, y: 2 }, { x, y: 2 }, shift);
    editor.finishGesture("commit");
}

Deno.test("cancelled prop movement replaces snapshots and layer actions resolve current IDs", () => {
    for(const outcome of ["cancel", "escape"] as const)
    {
        const editor = fixture();
        const oldLayer = editor.getLayer(1);
        editor.setTool("select");
        select(editor, 2, false);
        editor.beginSelection(1, { x: 2, y: 2 }, { x: 100, y: 100 }, false);
        editor.moveSelection(1, { x: 4, y: 4 }, { x: 200, y: 200 });
        editor.document.view.scrollLeft = 42;
        if(outcome === "escape") { editor.escape(); }
        else { editor.finishGesture("cancel"); }
        assert.notEqual(editor.getLayer(1), oldLayer);
        assert.equal(editor.document.placements[0].x, 2);
        assert.equal(editor.document.view.scrollLeft, 42);
        assert.deepEqual([...editor.selectedPlacementIds], [0]);
        const sheet = editor.document.tileSheets[0];
        editor.changeLayerMaterial(oldLayer.id, sheet);
        assert.equal(editor.getLayer(1).sheet, sheet);
        assert.notEqual(oldLayer.sheet, sheet);
        editor.moveLayer(oldLayer.id, 1);
        assert.deepEqual(editor.document.layers.map((layer) => layer.id), [0, 2, 1]);
        editor.removeLayer(oldLayer.id);
        assert.deepEqual(editor.document.layers.map((layer) => layer.id), [0, 2]);
        assert.equal(editor.document.activeLayerId, 2);
        editor.changeHistory(false);
        assert.deepEqual(editor.document.layers.map((layer) => layer.id), [0, 2, 1]);
    }
});

Deno.test("group movement and deletion form single undo actions and save only committed state", () => {
    const editor = fixture();
    editor.setTool("select");
    select(editor, 2, false);
    select(editor, 6, true);
    editor.beginSelection(1, { x: 2, y: 2 }, { x: 100, y: 100 }, false);
    for(const x of [3, 4, 5])
    {
        editor.moveSelection(1, { x, y: 3 }, { x, y: 3 });
        assert.equal(editor.committedDocument.placements[0].x, 2);
    }
    assert.equal(editor.finishGesture("commit")?.changed, true);
    assert.deepEqual(editor.document.placements.map((prop) => prop.x), [5, 9, 10]);
    assert.deepEqual([...editor.selectedPlacementIds], [0, 1]);
    editor.changeHistory(false);
    assert.deepEqual(editor.document.placements.map((prop) => prop.x), [2, 6, 10]);
    assert.equal(editor.history.undoLabel, null);
    editor.changeHistory(true);
    editor.deleteSelectedObjects();
    assert.deepEqual(editor.document.placements.map((prop) => prop.id), [2]);
    editor.changeHistory(false);
    assert.deepEqual(editor.document.placements.map((prop) => prop.id), [0, 1, 2]);
});

Deno.test("painting interpolates, clears higher layers, cancels and commits whole strokes", () => {
    const editor = fixture();
    editor.document.layers[2].painted.fill(true);
    editor.beginPaint(7, false);
    editor.paintTo(7, 0);
    editor.paintTo(7, 3);
    assert.deepEqual(editor.getLayer(1).painted.slice(0, 5), [true, true, true, true, false]);
    assert.deepEqual(editor.getLayer(2).painted.slice(0, 5), [false, false, false, false, true]);
    assert.equal(editor.committedDocument.layers[1].painted[0], false);
    editor.finishGesture("cancel");
    assert.equal(editor.getLayer(1).painted[0], false);
    assert.equal(editor.getLayer(2).painted[0], true);
    assert.equal(editor.history.undoLabel, null);
    assert.equal(editor.finishGesture("cancel"), null);
    editor.beginPaint(7, false);
    editor.paintTo(7, 0);
    editor.paintTo(7, -1);
    editor.paintTo(7, 3);
    editor.finishGesture("commit");
    assert.deepEqual(editor.getLayer(1).painted.slice(0, 4), [true, false, false, true]);
    editor.changeHistory(false);
    assert.equal(editor.getLayer(1).painted[3], false);
    editor.changeHistory(true);
    editor.beginPaint(7, true);
    editor.paintTo(7, 3);
    editor.finishGesture("commit");
    assert.equal(editor.getLayer(1).painted[3], false);
    editor.chooseLayer(0);
    assert.throws(() => editor.beginPaint(7, true), /base cannot be cleared/);
    assert.equal(editor.activeGesture, null);
    assert.equal(editor.history.inTransaction, false);
});

Deno.test("only one gesture or edit may own an active transaction", () => {
    const editor = fixture();
    editor.beginPaint(1, false);
    assert.throws(() => editor.beginPan(2, { x: 0, y: 0 }), /Finish the current gesture/);
    assert.throws(() => editor.removeLayer(1), /Finish the current gesture/);
    assert.throws(() => editor.changeHistory(false), /Finish the current gesture/);
    assert.throws(() => editor.setTool("select"), /Finish the current gesture/);
    assert.equal(editor.paintTo(2, 0), null);
    editor.escape();
    assert.equal(editor.tool, "select");
    assert.equal(editor.history.inTransaction, false);
    editor.beginPan(2, { x: 10, y: 20 });
    assert.deepEqual(editor.panTo(2, { x: 5, y: 10 }, { x: 100, y: 100 }), { x: 105, y: 110 });
    editor.document.view.scrollLeft = 105;
    editor.finishGesture("cancel");
    assert.equal(editor.document.view.scrollLeft, 105);
    assert.equal(editor.history.undoLabel, null);
});

Deno.test("selection supports shift toggles, additive boxes, cancellation and click collapse", () => {
    const editor = fixture();
    editor.setTool("select");
    select(editor, 10, false);
    editor.beginSelection(1, { x: 1, y: 1 }, { x: 50, y: 50 }, true);
    editor.moveSelection(1, { x: 7, y: 3 }, { x: 350, y: 150 });
    assert.deepEqual([...editor.selectedPlacementIds], [2, 0, 1]);
    assert(editor.selectionBox);
    editor.escape();
    assert.deepEqual([...editor.selectedPlacementIds], [2]);
    assert.equal(editor.selectionBox, null);
    select(editor, 2, true);
    select(editor, 10, true);
    assert.deepEqual([...editor.selectedPlacementIds], [0]);
    select(editor, 6, true);
    editor.beginSelection(1, { x: 2, y: 2 }, { x: 100, y: 100 }, false);
    editor.moveSelection(1, { x: -100, y: -100 }, { x: -500, y: -500 });
    editor.finishGesture("commit");
    assert.deepEqual(editor.document.placements.slice(0, 2).map(({ x, y }) => ({ x, y })),
        [{ x: -32, y: -32 }, { x: -28, y: -32 }]);
    editor.changeHistory(false);
    select(editor, 2, false);
    assert.deepEqual([...editor.selectedPlacementIds], [0]);
    assert.equal(editor.history.undoLabel, null);
    assert.equal(editor.history.redoLabel, "Move objects");
    editor.escape();
    assert.equal(editor.selectedPlacementIds.size, 0);
});

Deno.test("palette modes and placement keep selection transient while Escape always selects", () => {
    const editor = fixture();
    assert.equal(editor.tool, "select");
    assert.equal(editor.selectedPlacementIds.size, 0);
    editor.chooseDefinition(editor.document.propTypes[0]);
    assert.equal(editor.tool, "place");
    editor.placeObject({ x: 18, y: 18 });
    const id = editor.document.placements.at(-1)!.id;
    editor.escape();
    assert.equal(editor.tool, "select");
    assert.deepEqual([...editor.selectedPlacementIds], [id]);
    editor.chooseLayer(2);
    assert.equal(editor.tool, "terrain");
    assert.equal(editor.selectedPlacementIds.size, 0);
    editor.escape();
    assert.equal(editor.tool, "select");
    editor.changeHistory(false);
    assert.equal(editor.document.placements.length, 3);
    assert.equal(editor.document.activeLayerId, 2);
});

Deno.test("failed edits restore a usable document, selection and history", () => {
    const editor = fixture();
    select(editor, 2, false);
    assert.throws(() => editor.edit("Invalid", (project) => { project.placements[0].x = NaN; }));
    assert.equal(editor.document.placements[0].x, 2);
    assert.deepEqual([...editor.selectedPlacementIds], [0]);
    assert.equal(editor.history.inTransaction, false);
    assert.equal(editor.history.undoLabel, null);
    assert.throws(() => editor.removeLayer(999), /Missing layer/);
    assert.deepEqual(editor.document.layers.map((layer) => layer.id), [0, 1, 2]);
    editor.deleteSelectedObjects();
    assert.equal(editor.document.placements.length, 2);
});

Deno.test("shrinking crops by coordinates, removes only out-of-bounds centers, and undoes as one edit", () => {
    const editor = fixture();
    const project = editor.document;
    project.layers[1].painted[32 * 64 + 33] = true;
    project.layers[1].painted[33 * 64 + 32] = true;
    project.layers[1].painted[32 * 64 + 34] = true;
    project.layers[2].painted[34 * 64 + 32] = true;
    project.placements.push({ ...project.placements[0], id: 3, x: -2, y: -2 });
    for(const [x, y] of [[-2.001, 0], [2.001, 0], [0, -2.001], [0, 2.001]])
    {
        project.placements.push({ ...project.placements[0], id: project.placements.length, x, y });
    }
    project.nextPlacementId = project.placements.length;
    select(editor, 6, false);
    const before = cloneDocument(project);
    const result = editor.resizeMap(4);
    assert(result.changed);
    assert.equal(result.previous.gridSize, 64);
    assert.equal(editor.document.gridSize, 4);
    assert.deepEqual(editor.document.layers[1].painted.flatMap((value, cell) => value ? [cell] : []), [11, 14]);
    assert(editor.document.layers[0].painted.every(Boolean));
    assert(editor.document.layers[2].painted.every((value) => !value));
    assert.deepEqual(editor.document.placements.map((prop) => prop.id), [0, 3]);
    assert.equal(editor.document.placements[1].x, -2);
    assert.equal(editor.selectedPlacementIds.size, 0);
    assert.equal(editor.history.undoLabel, "Resize map");
    validateDocument(editor.document);
    editor.changeHistory(false);
    assert(sameContent(editor.document, before));
    assert.equal(editor.history.undoLabel, null);
    editor.changeHistory(true);
    assert.equal(editor.document.gridSize, 4);
    assert.deepEqual(editor.document.placements.map((prop) => prop.id), [0, 3]);
});

Deno.test("growing remaps row strides and fills only new base terrain without moving props", () => {
    const editor = fixture();
    editor.resizeMap(4);
    editor.document.layers[1].painted[7] = true;
    editor.history.clear();
    const before = cloneDocument(editor.document);
    editor.resizeMap(8);
    assert.equal(editor.document.layers[1].painted[29], true);
    assert.equal(editor.document.layers[1].painted.filter(Boolean).length, 1);
    assert.equal(editor.document.layers[0].painted.length, 64);
    assert(editor.document.layers[0].painted.every(Boolean));
    assert(editor.document.layers[2].painted.every((value) => !value));
    assert.deepEqual(editor.document.placements, before.placements);
    editor.changeHistory(false);
    assert(sameContent(editor.document, before));
    editor.changeHistory(true);
    assert.equal(editor.document.gridSize, 8);
    assert.equal(editor.document.layers[1].painted[29], true);
});

Deno.test("resize validates before editing, preserves redo for no-ops, and rejects active gestures", () => {
    const editor = fixture();
    const before = cloneDocument(editor.document);
    for(const size of [0, 1, 3, 6, 511, -1, 513, 1.5, NaN, Infinity])
    {
        assert.throws(() => editor.resizeMap(size), /Map size must/);
        assert(sameContent(editor.document, before));
        assert.equal(editor.history.inTransaction, false);
    }
    assert.equal(editor.resizeMap(64).changed, false);
    assert.equal(editor.history.undoLabel, null);
    editor.resizeMap(2);
    validateDocument(editor.document);
    assert.equal(editor.document.placements.length, 0);
    editor.changeHistory(false);
    assert.equal(editor.resizeMap(64).changed, false);
    assert.equal(editor.history.redoLabel, "Resize map");
    editor.beginPaint(1, false);
    assert.throws(() => editor.resizeMap(2), /Finish the current gesture/);
    editor.finishGesture("cancel");
});

Deno.test("resized maps constrain painting, placement, box selection and group movement", () => {
    const editor = fixture();
    editor.resizeMap(2);
    editor.document.layers[2].painted.fill(true);
    editor.beginPaint(1, false);
    assert.deepEqual(editor.paintTo(1, 0), { left: 0, top: 0, right: 2, bottom: 2 });
    editor.paintTo(1, 3);
    assert.equal(editor.paintTo(1, 4), null);
    assert.deepEqual(editor.document.layers[1].painted, [true, false, false, true]);
    assert.deepEqual(editor.document.layers[2].painted, [false, true, true, false]);
    editor.finishGesture("commit");
    editor.placeObject({ x: 10, y: -10 });
    const placed = editor.document.placements.at(-1)!;
    assert.deepEqual({ x: placed.x, y: placed.y }, { x: 1, y: -1 });
    editor.setTool("select");
    editor.beginSelection(1, { x: 0, y: 0 }, { x: 10, y: 10 }, false);
    editor.moveSelection(1, { x: 10, y: -10 }, { x: 500, y: 500 });
    assert.deepEqual(editor.selectionBox?.end, { x: 1, y: -1 });
    assert.deepEqual([...editor.selectedPlacementIds], [placed.id]);
    editor.finishGesture("commit");
    editor.beginSelection(1, { x: 1, y: -1 }, { x: 100, y: 100 }, false);
    editor.moveSelection(1, { x: 20, y: 20 }, { x: 1000, y: 1000 });
    assert.equal(editor.document.placements[0].x, 1);
    assert.equal(editor.document.placements[0].y, 1);
    editor.finishGesture("commit");
    validateDocument(editor.document);
});

Deno.test("resize accepts 512 cells per side and restores the prior size through undo", () => {
    const editor = fixture();
    const before = cloneDocument(editor.document);
    assert.equal(editor.resizeMap(512).changed, true);
    validateDocument(editor.document);
    assert.equal(editor.document.gridSize, 512);
    assert.equal(editor.document.layers[0].painted.length, 512 * 512);
    assert.equal(editor.document.layers[0].painted.at(-1), true);
    assert.equal(editor.document.layers[1].painted.at(-1), false);
    editor.changeHistory(false);
    assert(sameContent(editor.document, before));
    editor.changeHistory(true);
    assert.equal(editor.document.gridSize, 512);
});

Deno.test("cell coordinates round during placement and dragging, including undo and cancellation", () => {
    const editor = fixture();
    editor.placeObject({ x: 16.12349, y: 18.45678 });
    assert.equal(editor.document.placements.at(-1)!.x, 16.123);
    assert.equal(editor.document.placements.at(-1)!.y, 18.457);
    editor.setTool("select");
    select(editor, 2, false);
    select(editor, 6, true);
    editor.beginSelection(1, { x: 2, y: 2 }, { x: 100, y: 100 }, false);
    for(const offset of [0.12349, 0.22349])
    {
        editor.moveSelection(1, { x: 2 + offset, y: 2.45678 }, { x: 120, y: 120 });
        assert.equal(editor.document.placements[0].x, Math.round((2 + offset) * 1000) / 1000);
        assert.equal(editor.document.placements[1].x, Math.round((6 + offset) * 1000) / 1000);
        assert.equal(editor.document.placements[0].y, 2.457);
    }
    editor.finishGesture("commit");
    editor.changeHistory(false);
    assert.equal(editor.document.placements[0].x, 2);
    editor.changeHistory(true);
    assert.equal(editor.document.placements[0].x, 2.223);
    assert.equal(editor.document.placements[1].x, 6.223);
    editor.beginSelection(1, { x: 2.223, y: 2.457 }, { x: 100, y: 100 }, false);
    editor.moveSelection(1, { x: -100, y: 5000 }, { x: 200, y: 200 });
    assert.equal(editor.document.placements[0].x, -32);
    assert.equal(editor.document.placements[0].y, 32);
    assert.equal(editor.document.placements[1].x, -28);
    editor.finishGesture("cancel");
    assert.equal(editor.document.placements[0].x, 2.223);
    assert.equal(editor.document.placements[0].y, 2.457);
});

Deno.test("centered growth and shrink preserve cell alignment and prop coordinates at every allowed size", () => {
    const editor = fixture();
    editor.resizeMap(2);
    editor.document.layers[1].painted[0] = true;
    editor.placeObject({ x: -0.5, y: 0.5 });
    const original = cloneDocument(editor.document);
    for(const size of [4, 8, 16, 32, 64, 128, 256, 512])
    {
        editor.resizeMap(size);
        const index = (size / 2 - 1) * size + size / 2 - 1;
        assert.equal(editor.document.layers[1].painted[index], true);
        assert.equal(editor.document.layers[1].painted.filter(Boolean).length, 1);
        assert.deepEqual(editor.document.placements, original.placements);
        editor.resizeMap(2);
        assert(sameContent(editor.document, original));
    }
});

Deno.test("actors share selection, drag cancellation, deletion and resize history with props", () => {
    const editor = fixture();
    const project = editor.document;
    project.actorTypes = [{ kind: "actor", name: "Grave", class: "Tombstone", sprite: project.sprites[0], scale: 1 }];
    const actor = project.actorTypes[0];
    editor.chooseDefinition(actor);
    editor.placeObject({ x: 6, y: 2 });
    assert.equal(editor.document.placements.at(-1)!.definition, actor);
    const actorId = editor.document.placements.at(-1)!.id;
    editor.setTool("select");
    select(editor, 6, false);
    assert.deepEqual([...editor.selectedPlacementIds], [actorId]); // Actor is above the prop at the same position.
    select(editor, 2, true);
    editor.beginSelection(1, { x: 6, y: 2 }, { x: 0, y: 0 }, false);
    editor.moveSelection(1, { x: 7, y: 3 }, { x: 10, y: 10 });
    assert.equal(editor.document.placements[0].x, 3);
    assert.equal(editor.document.placements.at(-1)!.x, 7);
    editor.finishGesture("cancel");
    assert.equal(editor.document.placements[0].x, 2);
    assert.equal(editor.document.placements.at(-1)!.x, 6);
    assert.equal(editor.selectedPlacementIds.size, 2);
    editor.deleteSelectedObjects();
    assert.equal(editor.document.placements.length, 2);
    editor.changeHistory(false);
    assert.equal(editor.document.placements.length, 4);
    assert.equal(editor.document.placements.at(-1)!.definition, actor);
    editor.resizeMap(4);
    assert.equal(editor.document.placements.length, 1);
    editor.changeHistory(false);
    assert.equal(editor.document.placements.length, 4);
    assert.equal(editor.document.placements.at(-1)!.definition, actor);
});

Deno.test("new props remain below actors and placement requires a catalog definition", () => {
    const editor = fixture();
    const project = editor.document;
    project.actorTypes = [{ kind: "actor", name: "Grave", class: "Tombstone", sprite: project.sprites[0], scale: 1 }];
    editor.chooseDefinition(project.actorTypes[0]);
    editor.placeObject({ x: 2, y: 2 });
    const actorId = project.placements.at(-1)!.id;
    editor.chooseDefinition(project.propTypes[0]);
    editor.placeObject({ x: 2, y: 2 });
    assert.equal(project.placements.at(-1)!.id, actorId);
    const reordered = cloneDocument(project);
    reordered.placements.reverse();
    assert.throws(() => validateDocument(reordered), /placement order/);
    editor.setTool("select");
    select(editor, 2, false);
    assert.deepEqual([...editor.selectedPlacementIds], [actorId]);
    assert.throws(() => editor.chooseDefinition({ ...project.actorTypes[0] }), /not in this project/);
    const empty = cloneDocument(project);
    empty.propTypes = [];
    empty.actorTypes = [];
    empty.placements = [];
    const noDefinitions = new Editor(empty, new History(100, 64 * 1024 * 1024));
    assert.equal(noDefinitions.selectedDefinition, null);
    assert.throws(() => noDefinitions.placeObject({ x: 0, y: 0 }), /definition/);
});

Deno.test("single props and actors scale absolutely around rotated pivots with undo, reset and cancellation", () => {
    for(const kind of ["prop", "actor"] as const)
    {
        const editor = fixture();
        const project = editor.document;
        const sequence = { ...project.sequences[0], pivotX: 0, pivotY: 1 };
        project.sequences = [sequence];
        const sprite = { name: "Scaled", sequences: [sequence] };
        project.sprites = [sprite];
        const definition = kind === "actor" ? { kind, name: "Actor", class: "Tombstone", sprite, scale: 0.25 }
            : { kind, name: "Prop", sprite, scale: 0.25 };
        project.propTypes = definition.kind === "prop" ? [definition] : [];
        project.actorTypes = definition.kind === "actor" ? [definition] : [];
        project.placements = [{ id: 0, definition, x: 2, y: 2, rotation: 90 }];
        select(editor, 2, false);
        const handle = scaleHandles(project.placements[0])[1];
        assert(editor.beginScale(2, handle, { x: 100, y: 100 }, 4));
        const twice = { x: 2 + 2 * (handle.x - 2), y: 2 + 2 * (handle.y - 2) };
        editor.scaleTo(99, twice, { x: 120, y: 120 });
        assert.equal(editor.document.placements[0].scale, undefined);
        editor.scaleTo(2, twice, { x: 120, y: 120 });
        assert.equal(editor.document.placements[0].scale, 0.5);
        assert.equal(editor.committedDocument.placements[0].scale, undefined);
        assert(editor.finishGesture("commit")?.changed);
        assert.deepEqual([editor.document.placements[0].x, editor.document.placements[0].y,
            editor.document.placements[0].rotation], [2, 2, 90]);
        editor.changeHistory(false);
        assert(!Object.hasOwn(editor.document.placements[0], "scale"));
        editor.changeHistory(true);
        assert.equal(editor.document.placements[0].scale, 0.5);
        assert(editor.resetScale().changed);
        assert(!Object.hasOwn(editor.document.placements[0], "scale"));
        for(const explicit of [undefined, 0.5])
        {
            if(explicit === undefined) { delete editor.document.placements[0].scale; }
            else { editor.document.placements[0].scale = explicit; }
            const start = scaleHandles(editor.document.placements[0])[1];
            assert(editor.beginScale(3, start, { x: 0, y: 0 }, 4));
            editor.scaleTo(3, { x: 4 - start.x, y: 4 - start.y }, { x: 20, y: 20 });
            assert.equal(editor.document.placements[0].scale, 0.001);
            editor.escape();
            assert.equal(editor.document.placements[0].scale, explicit);
            assert.equal(Object.hasOwn(editor.document.placements[0], "scale"), explicit !== undefined);
        }
    }
});

Deno.test("scale handles require a single selection and clicks do not create overrides", () => {
    const editor = fixture();
    const handle = scaleHandles(editor.document.placements[0])[0];
    assert(!editor.beginScale(1, handle, handle, 1));
    select(editor, 2, false);
    select(editor, 6, true);
    assert(!editor.beginScale(1, handle, handle, 1));
    select(editor, 2, false);
    assert(editor.beginScale(1, handle, handle, 1));
    editor.scaleTo(1, handle, handle);
    assert.equal(editor.finishGesture("commit")?.changed, false);
    assert(!Object.hasOwn(editor.document.placements[0], "scale"));
    assert.equal(editor.history.undoLabel, null);
});

Deno.test("numeric absolute and percentage scales apply once to props and actors with undo", () => {
    for(const kind of ["prop", "actor"] as const)
    {
        const editor = fixture();
        const project = editor.document;
        const sequence = { ...project.sequences[0] };
        const sprite = { name: "Numeric", sequences: [sequence] };
        const definition = kind === "actor" ? { kind, name: "Actor", class: "Tombstone", sprite, scale: 0.25 }
            : { kind, name: "Prop", sprite, scale: 0.25 };
        project.sequences = [sequence];
        project.sprites = [sprite];
        project.propTypes = definition.kind === "prop" ? [definition] : [];
        project.actorTypes = definition.kind === "actor" ? [definition] : [];
        project.placements = [{ id: 0, definition, x: 2, y: 2, rotation: 45 }];
        select(editor, 2, false);
        assert(editor.setScale(150, "percent").changed);
        assert.equal(editor.document.placements[0].scale, 0.375);
        assert(editor.setScale(200, "percent").changed);
        assert.equal(editor.document.placements[0].scale, 0.75);
        assert(editor.setScale(0.5, "absolute").changed);
        assert.equal(editor.document.placements[0].scale, 0.5);
        assert.equal(definition.scale, 0.25);
        assert.deepEqual([editor.document.placements[0].x, editor.document.placements[0].y,
            editor.document.placements[0].rotation], [2, 2, 45]);
        assert.equal(editor.setScale(0.5, "absolute").changed, false);
        for(const scale of [0.75, 0.375, undefined])
        {
            editor.changeHistory(false);
            assert.equal(editor.document.placements[0].scale, scale);
        }
        assert(!Object.hasOwn(editor.document.placements[0], "scale"));
        assert.equal(editor.changeHistory(false), null);
        editor.changeHistory(true);
        assert.equal(editor.document.placements[0].scale, 0.375);
        editor.resetScale();
        assert(!Object.hasOwn(editor.document.placements[0], "scale"));
        editor.changeHistory(false);
        assert.equal(editor.document.placements[0].scale, 0.375);
    }
});

Deno.test("numeric scale rejects invalid results and non-single selections without edits", () => {
    const editor = fixture();
    assert.throws(() => editor.setScale(1, "absolute"), /Select one/);
    select(editor, 2, false);
    select(editor, 6, true);
    assert.throws(() => editor.setScale(150, "percent"), /Select one/);
    select(editor, 2, false);
    const before = cloneDocument(editor.document);
    for(const value of [0, -1, NaN, Infinity, -Infinity])
    {
        for(const mode of ["absolute", "percent"] as const)
        {
            assert.throws(() => editor.setScale(value, mode), /greater than zero/);
            assert(sameContent(editor.document, before));
            assert.equal(editor.history.inTransaction, false);
            assert.equal(editor.history.undoLabel, null);
        }
    }
    editor.setScale(1000, "absolute");
    assert.throws(() => editor.setScale(Number.MAX_VALUE, "percent"), /resulting scale/);
    assert.equal(editor.document.placements[0].scale, 1000);
    editor.setScale(Number.MIN_VALUE, "absolute");
    assert.throws(() => editor.setScale(0.001, "percent"), /resulting scale/);
    assert.equal(editor.document.placements[0].scale, Number.MIN_VALUE);
});

Deno.test("only committed handle clicks request numeric controls, never cancelled clicks or drags", () => {
    const editor = fixture();
    select(editor, 2, false);
    const handle = scaleHandles(editor.document.placements[0])[0];
    assert(editor.beginScale(1, handle, { x: 100, y: 100 }, 4));
    editor.scaleTo(1, handle, { x: 101, y: 101 });
    const click = editor.finishGesture("commit");
    assert.equal(click?.clickedScaleHandle, true);
    assert.equal(click?.changed, false);
    assert.equal(editor.history.undoLabel, null);
    assert(editor.beginScale(1, handle, { x: 100, y: 100 }, 4));
    assert.equal(editor.finishGesture("cancel")?.clickedScaleHandle, false);
    assert(editor.beginScale(1, handle, { x: 100, y: 100 }, 4));
    editor.scaleTo(1, { x: handle.x - 1, y: handle.y + 1 }, { x: 80, y: 120 });
    editor.scaleTo(1, handle, { x: 100, y: 100 });
    assert.equal(editor.finishGesture("commit")?.clickedScaleHandle, false);
    assert(editor.beginScale(1, handle, { x: 100, y: 100 }, 4));
    assert.equal(editor.escape()?.clickedScaleHandle, false);
});

import { strict as assert } from "node:assert";
import { Editor } from "./editor.ts";
import { History } from "./history.ts";
import { createDocument, type ImageAsset, type PropAsset } from "./document.ts";
import { kCellCount } from "./grid.ts";

function fixture(): Editor
{
    const tile: ImageAsset = { kind: "tile", id: "assets/tiles/a.png", label: "a",
        blob: new Blob(), url: "", dimension: 256, pixels: new Uint8ClampedArray(), coverage: new Uint8Array() };
    const other = { ...tile, id: "assets/tiles/b.png" };
    const project = createDocument([tile, other], [{ ...tile, kind: "mask", id: "assets/masks/a.png" }]);
    project.layers.push({ id: 2, sheet: tile, painted: Array(kCellCount).fill(false) });
    project.nextLayerId = 3;
    const asset: PropAsset = { kind: "prop", id: "assets/props/a.png", label: "a",
        blob: new Blob(), url: "", width: 20, height: 20, image: {} as CanvasImageSource };
    project.propAssets = [asset];
    project.props = [100, 300, 500].map((x, id) => ({ id, asset, x, y: 100, rotation: 0 }));
    project.nextPropId = 3;
    return new Editor(project, new History(100, 64 * 1024 * 1024));
}

function select(editor: Editor, x: number, shift: boolean): void
{
    editor.beginSelection(1, { x, y: 100 }, { x, y: 100 }, shift);
    editor.finishGesture("commit");
}

Deno.test("cancelled prop movement replaces snapshots and layer actions resolve current IDs", () => {
    for(const outcome of ["cancel", "escape"] as const)
    {
        const editor = fixture();
        const oldLayer = editor.getLayer(1);
        editor.setTool("select");
        select(editor, 100, false);
        editor.beginSelection(1, { x: 100, y: 100 }, { x: 100, y: 100 }, false);
        editor.moveSelection(1, { x: 200, y: 200 }, { x: 200, y: 200 });
        editor.document.view.scrollLeft = 42;
        if(outcome === "escape") { editor.escape(); }
        else { editor.finishGesture("cancel"); }
        assert.notEqual(editor.getLayer(1), oldLayer);
        assert.equal(editor.document.props[0].x, 100);
        assert.equal(editor.document.view.scrollLeft, 42);
        assert.deepEqual([...editor.selectedPropIds], [0]);
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
    select(editor, 100, false);
    select(editor, 300, true);
    editor.beginSelection(1, { x: 100, y: 100 }, { x: 100, y: 100 }, false);
    for(const x of [150, 200, 250])
    {
        editor.moveSelection(1, { x, y: 150 }, { x, y: 150 });
        assert.equal(editor.committedDocument.props[0].x, 100);
    }
    assert.equal(editor.finishGesture("commit")?.changed, true);
    assert.deepEqual(editor.document.props.map((prop) => prop.x), [250, 450, 500]);
    assert.deepEqual([...editor.selectedPropIds], [0, 1]);
    editor.changeHistory(false);
    assert.deepEqual(editor.document.props.map((prop) => prop.x), [100, 300, 500]);
    assert.equal(editor.history.undoLabel, null);
    editor.changeHistory(true);
    editor.deleteSelectedProps();
    assert.deepEqual(editor.document.props.map((prop) => prop.id), [2]);
    editor.changeHistory(false);
    assert.deepEqual(editor.document.props.map((prop) => prop.id), [0, 1, 2]);
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
    select(editor, 500, false);
    editor.beginSelection(1, { x: 50, y: 50 }, { x: 50, y: 50 }, true);
    editor.moveSelection(1, { x: 350, y: 150 }, { x: 350, y: 150 });
    assert.deepEqual([...editor.selectedPropIds], [2, 0, 1]);
    assert(editor.selectionBox);
    editor.escape();
    assert.deepEqual([...editor.selectedPropIds], [2]);
    assert.equal(editor.selectionBox, null);
    select(editor, 100, true);
    select(editor, 500, true);
    assert.deepEqual([...editor.selectedPropIds], [0]);
    select(editor, 300, true);
    editor.beginSelection(1, { x: 100, y: 100 }, { x: 100, y: 100 }, false);
    editor.moveSelection(1, { x: -500, y: -500 }, { x: -500, y: -500 });
    editor.finishGesture("commit");
    assert.deepEqual(editor.document.props.slice(0, 2).map(({ x, y }) => ({ x, y })),
        [{ x: 0, y: 0 }, { x: 200, y: 0 }]);
    editor.changeHistory(false);
    select(editor, 100, false);
    assert.deepEqual([...editor.selectedPropIds], [0]);
    assert.equal(editor.history.undoLabel, null);
    assert.equal(editor.history.redoLabel, "Move props");
    editor.escape();
    assert.equal(editor.selectedPropIds.size, 0);
});

Deno.test("palette modes and placement keep selection transient while Escape always selects", () => {
    const editor = fixture();
    editor.chooseProp(editor.document.propAssets[0]);
    assert.equal(editor.tool, "place");
    editor.placeProp({ x: 900, y: 900 });
    const id = editor.document.props.at(-1)!.id;
    editor.escape();
    assert.equal(editor.tool, "select");
    assert.deepEqual([...editor.selectedPropIds], [id]);
    editor.chooseLayer(2);
    assert.equal(editor.tool, "terrain");
    assert.equal(editor.selectedPropIds.size, 0);
    editor.escape();
    assert.equal(editor.tool, "select");
    editor.changeHistory(false);
    assert.equal(editor.document.props.length, 3);
    assert.equal(editor.document.activeLayerId, 2);
});

Deno.test("failed edits restore a usable document, selection and history", () => {
    const editor = fixture();
    select(editor, 100, false);
    assert.throws(() => editor.edit("Invalid", (project) => { project.props[0].x = NaN; }));
    assert.equal(editor.document.props[0].x, 100);
    assert.deepEqual([...editor.selectedPropIds], [0]);
    assert.equal(editor.history.inTransaction, false);
    assert.equal(editor.history.undoLabel, null);
    assert.throws(() => editor.removeLayer(999), /Missing layer/);
    assert.deepEqual(editor.document.layers.map((layer) => layer.id), [0, 1, 2]);
    editor.deleteSelectedProps();
    assert.equal(editor.document.props.length, 2);
});

Deno.test("prop coordinates round during placement and group dragging, including undo and cancellation", () => {
    const editor = fixture();
    editor.placeProp({ x: 800.12349, y: 900.45678 });
    assert.equal(editor.document.props.at(-1)!.x, 800.123);
    assert.equal(editor.document.props.at(-1)!.y, 900.457);
    editor.setTool("select");
    select(editor, 100, false);
    select(editor, 300, true);
    editor.beginSelection(1, { x: 100, y: 100 }, { x: 100, y: 100 }, false);
    for(const offset of [10.12349, 20.12349])
    {
        editor.moveSelection(1, { x: 100 + offset, y: 110.45678 }, { x: 110, y: 110 });
        assert.equal(editor.document.props[0].x, Math.round((100 + offset) * 1000) / 1000);
        assert.equal(editor.document.props[1].x, Math.round((300 + offset) * 1000) / 1000);
        assert.equal(editor.document.props[0].y, 110.457);
    }
    editor.finishGesture("commit");
    editor.changeHistory(false);
    assert.equal(editor.document.props[0].x, 100);
    editor.changeHistory(true);
    assert.equal(editor.document.props[0].x, 120.123);
    assert.equal(editor.document.props[1].x, 320.123);
    editor.beginSelection(1, { x: 120.123, y: 110.457 }, { x: 100, y: 100 }, false);
    editor.moveSelection(1, { x: -100, y: 5000 }, { x: 200, y: 200 });
    assert.equal(editor.document.props[0].x, 0);
    assert.equal(editor.document.props[0].y, 4096);
    assert.equal(editor.document.props[1].x, 200);
    editor.finishGesture("cancel");
    assert.equal(editor.document.props[0].x, 120.123);
    assert.equal(editor.document.props[0].y, 110.457);
});

import { History } from "./history.ts";
import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset, type PropAsset, type Prop } from "./document.ts";
import { clampPropPoint, hitProp, mapPoint, propsInBox, groupMoveDelta } from "./props.ts";

const asset: PropAsset = { kind: "prop", id: "assets/props/stone.png", label: "stone.png",
    blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource };
const prop: Prop = { id: 0, asset, x: 100, y: 200, rotation: 90 };

Deno.test("prop picking respects rotation, native rectangular bounds and placement order", () => {
    assert.equal(hitProp([prop], 100, 235), prop);
    assert.equal(hitProp([prop], 135, 200), null);
    assert.equal(hitProp([prop], 100, 241), null);
    const upper = { ...prop, id: 1, rotation: 0 };
    assert.equal(hitProp([prop, upper], 100, 200), upper);
    assert.equal(hitProp([prop, upper], 100, 230), prop);
    assert.equal(hitProp([], 100, 200), null);
    const diagonal = { ...prop, rotation: 45 };
    assert.equal(hitProp([diagonal], 120, 220), diagonal);
    assert.equal(hitProp([diagonal], 120, 180), null);
});

Deno.test("pointer conversion handles zoom and viewport offsets, and bounds constrain centers", () => {
    assert.deepEqual(mapPoint(210, 420, { left: 10, top: 20, width: 2048, height: 2048 }), { x: 400, y: 800 });
    assert.deepEqual(clampPropPoint(-20, 4200), { x: 0, y: 4096 });
    assert.deepEqual(clampPropPoint(200.5, 800.25), { x: 200.5, y: 800.25 });
});

Deno.test("box selection uses centers inclusively in either drag direction", () => {
    const props = [prop, { ...prop, id: 1, x: 200, y: 250 }, { ...prop, id: 2, x: 301, y: 250 }];
    const start = { x: 100, y: 200 }, end = { x: 300, y: 300 };
    assert.deepEqual(propsInBox(props, { start, end }), [0, 1]);
    assert.deepEqual(propsInBox(props, { start: end, end: start }), [0, 1]);
    assert.deepEqual(propsInBox(props, { start: { x: 110, y: 210 }, end: { x: 120, y: 220 } }), []);
    assert.deepEqual(propsInBox([], { start, end }), []);
});

Deno.test("group translations clamp uniformly at all edges without accumulating drift", () => {
    const points = [{ x: 20, y: 50 }, { x: 4000, y: 4050 }];
    assert.deepEqual(groupMoveDelta(points, -100, -100), { x: -20, y: -50 });
    assert.deepEqual(groupMoveDelta(points, 200, 200), { x: 96, y: 46 });
    assert.deepEqual(groupMoveDelta(points, 10.5, 20.5), { x: 10.5, y: 20.5 });
    assert.deepEqual(groupMoveDelta([], 200, 200), { x: 0, y: 0 });
    assert.deepEqual(groupMoveDelta(points, 0, 0), { x: 0, y: 0 });
});

Deno.test("group movement, cancellation and deletion preserve complete undo actions", () => {
    const terrain: ImageAsset = { kind: "tile", id: "assets/tiles/tile.png", label: "tile.png",
        blob: new Blob(), url: "", dimension: 256, pixels: new Uint8ClampedArray(), coverage: new Uint8Array() };
    let project = createDocument([terrain], [{ ...terrain, kind: "mask", id: "assets/masks/mask.png" }]);
    project.propAssets = [asset];
    project.props = [prop, { ...prop, id: 1, x: 300 }, { ...prop, id: 2, x: 500 }].map((item) => ({ ...item }));
    project.nextPropId = 3;
    const history = new History(100, 64 * 1024 * 1024);
    const selected = new Set([0, 1]);
    const origins = project.props.filter((item) => selected.has(item.id)).map((item) => ({ ...item }));
    history.begin(project, "Move props");
    for(const dx of [10, 100, 200])
    {
        const delta = groupMoveDelta(origins, dx, 50);
        for(const origin of origins) { Object.assign(project.props[origin.id], { x: origin.x + delta.x, y: origin.y + delta.y }); }
    }
    assert.equal(history.committedState(project).props[0].x, 100);
    assert(history.commit(project));
    assert.equal(project.props[2].x, 500);
    project = history.undo(project)!;
    assert.deepEqual(project.props.map((item) => item.x), [100, 300, 500]);
    project = history.redo(project)!;
    assert.deepEqual(project.props.map((item) => item.x), [300, 500, 500]);
    history.begin(project, "Cancelled move");
    project.props[0].x = 0;
    project.props[1].x = 200;
    project = history.rollback()!;
    assert.deepEqual(project.props.map((item) => item.x), [300, 500, 500]);
    history.begin(project, "Delete props");
    project.props = project.props.filter((item) => !selected.has(item.id));
    history.commit(project);
    assert.deepEqual(project.props.map((item) => item.id), [2]);
    project = history.undo(project)!;
    assert.deepEqual(project.props.map((item) => item.id), [0, 1, 2]);
});

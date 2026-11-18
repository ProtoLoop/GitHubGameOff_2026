import { strict as assert } from "node:assert";
import { type PropAsset, type Prop } from "./document.ts";
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

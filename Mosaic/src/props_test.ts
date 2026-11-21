import { strict as assert } from "node:assert";
import { type PropAsset, type Prop } from "./document.ts";
import { clampPropPoint, hitProp, mapPoint, mapPixels, propsInBox, groupMoveDelta, propImageRect } from "./props.ts";

const asset: PropAsset = { kind: "prop", id: "assets/props/stone.png", label: "stone.png",
    blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        srcRect: { x: 0, y: 0, w: 80, h: 20 },
        pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
const prop: Prop = { id: 0, asset, x: 100 / 64, y: -200 / 64, rotation: 90 };

Deno.test("prop picking respects rotation, native rectangular bounds and placement order", () => {
    assert.equal(hitProp([prop], 100 / 64, -235 / 64), prop);
    assert.equal(hitProp([prop], 135 / 64, -200 / 64), null);
    assert.equal(hitProp([prop], 100 / 64, -241 / 64), null);
    const upper = { ...prop, id: 1, rotation: 0 };
    assert.equal(hitProp([prop, upper], 100 / 64, -200 / 64), upper);
    assert.equal(hitProp([prop, upper], 100 / 64, -230 / 64), prop);
    assert.equal(hitProp([], 100 / 64, -200 / 64), null);
    const diagonal = { ...prop, rotation: 45 };
    assert.equal(hitProp([diagonal], 120 / 64, -220 / 64), diagonal);
    assert.equal(hitProp([diagonal], 120 / 64, -180 / 64), null);
});

Deno.test("picking follows scaled rectangles rotated about non-center pivots", () => {
    // A bottom-left pivot at (0, 0), scaled to 160 by 40 pixels and rotated clockwise 90 degrees.
    // Its world rectangle extends right by 40 pixels and down by 160 pixels from the fixed pivot.
    const anchored = { ...prop, x: 0, y: 0, asset: { ...asset, pivotX: 0, pivotY: 1, scale: 2 } };
    assert.equal(hitProp([anchored], 20 / 64, -80 / 64), anchored);
    assert.equal(hitProp([anchored], -1 / 64, -80 / 64), null);
    assert.equal(hitProp([anchored], 41 / 64, -80 / 64), null);
    assert.equal(hitProp([anchored], 20 / 64, 1 / 64), null);
    assert.equal(hitProp([anchored], 20 / 64, -161 / 64), null);
    const small = { ...anchored, rotation: 0, asset: { ...asset, pivotX: 1, pivotY: 0, scale: 0.25 } };
    assert.equal(hitProp([small], -10 / 64, -2 / 64), small);
    assert.equal(hitProp([small], 1 / 64, -2 / 64), null);
    assert.equal(hitProp([small], -21 / 64, -2 / 64), null);
    assert.equal(hitProp([small], -10 / 64, -6 / 64), null);
    assert.deepEqual(propsInBox([anchored], { start: { x: -0.01, y: -0.01 }, end: { x: 0.01, y: 0.01 } }), [prop.id]);
});

Deno.test("pointer conversion matches centered Y-up cells and reverses the drawing transform", () => {
    for(const size of [2, 4, 64, 512])
    {
        for(const zoom of [1 / 16, 0.25, 1, 4])
        {
            const pixels = size * 64;
            const bounds = { left: -30, top: 20, width: pixels * zoom, height: pixels * zoom };
            for(const point of [{ x: -size / 2, y: size / 2 }, { x: 0, y: 0 },
                { x: size / 2, y: -size / 2 }, { x: -0.5, y: 0.5 }])
            {
                const drawn = mapPixels(point, size);
                assert.deepEqual(mapPoint(bounds.left + drawn.x * zoom, bounds.top + drawn.y * zoom, bounds, size), point);
            }
            assert.deepEqual(mapPoint(bounds.left, bounds.top, bounds, size), { x: -size / 2, y: size / 2 });
            assert.deepEqual(mapPixels({ x: 0, y: 0 }, size), { x: pixels / 2, y: pixels / 2 });
        }
        assert.deepEqual(clampPropPoint(-size, size, size), { x: -size / 2, y: size / 2 });
        assert.deepEqual(clampPropPoint(size, -size, size), { x: size / 2, y: -size / 2 });
    }
});

Deno.test("box selection includes centered coordinates in either drag direction", () => {
    const props = [{ ...prop, x: -1, y: -2 }, { ...prop, id: 1, x: 0, y: 0 }, { ...prop, id: 2, x: 2.01, y: 1 }];
    const start = { x: -1, y: -2 }, end = { x: 2, y: 2 };
    assert.deepEqual(propsInBox(props, { start, end }), [0, 1]);
    assert.deepEqual(propsInBox(props, { start: end, end: start }), [0, 1]);
    assert.deepEqual(propsInBox([], { start, end }), []);
});

Deno.test("group translations clamp on all four centered boundaries without deforming the group", () => {
    const points = [{ x: -31.5, y: -31 }, { x: 31, y: 31.5 }];
    assert.deepEqual(groupMoveDelta(points, -10, -10, 64), { x: -0.5, y: -1 });
    assert.deepEqual(groupMoveDelta(points, 10, 10, 64), { x: 1, y: 0.5 });
    assert.deepEqual(groupMoveDelta(points, 0.125, -0.25, 64), { x: 0.125, y: -0.25 });
    assert.deepEqual(groupMoveDelta([], 2, 2, 64), { x: 0, y: 0 });
    assert.deepEqual(groupMoveDelta(points, 0, 0, 64), { x: 0, y: 0 });
});

Deno.test("cropped sprite picking uses crop dimensions with scaled non-center pivots and rotation", () => {
    const cropped = { ...prop, x: 0, y: 0, rotation: 90,
        asset: { ...asset, srcRect: { x: 30, y: 4, w: 40, h: 10 }, pivotX: 0.25, pivotY: 1, scale: 2 } };
    assert.deepEqual(propImageRect(cropped.asset), { x: -20, y: -20, width: 80, height: 20 });
    // After rotation, the crop covers X [0, 20] and Y [-20, 60] in Y-down pixels.
    assert.equal(hitProp([cropped], 10 / 64, -30 / 64), cropped);
    for(const [x, y] of [[-1, 30], [21, 30], [10, -21], [10, 61]])
    {
        assert.equal(hitProp([cropped], x / 64, -y / 64), null);
    }
    // Moving the source rectangle within the texture does not move the sprite or its pivot.
    const moved = { ...cropped, asset: { ...cropped.asset, srcRect: { ...cropped.asset.srcRect, x: 0, y: 0 } } };
    assert.deepEqual(propImageRect(moved.asset), propImageRect(cropped.asset));
    assert.equal(hitProp([moved], 10 / 64, -30 / 64), moved);
});

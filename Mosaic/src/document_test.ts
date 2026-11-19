import { strict as assert } from "node:assert";
import { cellMasks } from "./placement.ts";
import { documentBytes, documentAssets, decodePropAsset, cloneDocument, createDocument, sameTerrainContent, sameContent, validateDocument, type ImageAsset, type PropAsset } from "./document.ts";

function asset(kind: "tile" | "mask"): ImageAsset
{
    return { id: `assets/${kind === "tile" ? "tiles" : "masks"}/${kind}.png`, kind, label: `${kind}.png`, blob: new Blob([kind], { type: "image/png" }),
        dimension: 256, pixels: new Uint8ClampedArray(4), coverage: new Uint8Array(0), url: `blob:${kind}` };
}

Deno.test("document snapshots isolate map changes and share immutable images", () => {
    const project = createDocument([asset("tile")], [asset("mask")]);
    const copy = cloneDocument(project);
    copy.layers[1].painted[100] = true;
    assert.equal(project.layers[1].painted[100], false);
    assert.equal(copy.layers[1].sheet, project.layers[1].sheet);
    assert.equal(copy.masks[0], project.masks[0]);
    assert.equal(sameContent(project, copy), false);
    copy.layers[1].painted[100] = false;
    copy.view.zoom = 2;
    copy.activeLayerId = 0;
    assert.equal(sameContent(project, copy), true);
});

Deno.test("document validation rejects invalid IDs, cells, masks, and view settings", () => {
    const original = createDocument([asset("tile")], [asset("mask")]);
    for(const corrupt of [
        (p: typeof original) => { p.layers[1].painted.pop(); },
        (p: typeof original) => { p.layers[0].painted[0] = false; },
        (p: typeof original) => { p.layers[1].painted[0] = 1 as unknown as boolean; },
        (p: typeof original) => { p.tileSheets = []; },
        (p: typeof original) => { p.tileSheets = [p.tileSheets[0], p.tileSheets[0]]; },
        (p: typeof original) => { p.layers.push(p.layers[1]); },
        (p: typeof original) => { p.nextLayerId = 1; },
        (p: typeof original) => { p.activeLayerId = 999; },
        (p: typeof original) => { p.maskMode = 1; },
        (p: typeof original) => { p.view.zoom = NaN; },
        (p: typeof original) => { p.masks = []; }
    ])
    {
        const project = cloneDocument(original);
        corrupt(project);
        assert.throws(() => validateDocument(project));
    }
    validateDocument(original);
});

Deno.test("documents accept image assets larger than 32 MiB", () => {
    const tile = { ...asset("tile"), blob: new Blob([new Uint8Array(32 * 1024 * 1024 + 1)], { type: "image/png" }) };
    const project = createDocument([tile], [asset("mask")]);
    validateDocument(project);
    assert.equal(project.tileSheets[0].blob, tile.blob);
});

Deno.test("snapshots preserve occupancy and reproduce mask choices", () => {
    const project = createDocument([asset("tile")], [asset("mask")]);
    project.layers[1].painted[65] = true;
    const copy = cloneDocument(project);
    assert.notEqual(copy.layers[1].painted, project.layers[1].painted);
    assert(sameContent(project, copy));
    assert.deepEqual(cellMasks(copy, copy.layers[1], 65), cellMasks(project, project.layers[1], 65));
    copy.layers[1].painted[65] = false;
    assert.equal(sameContent(project, copy), false);
    assert(project.layers[1].painted[65]);
});

Deno.test("terrain textures share dimensions and use multiples of 64 up to 4096", () => {
    for(const dimension of [64, 128, 192, 256, 1024, 4096])
    {
        const sheet = { ...asset("tile"), dimension };
        validateDocument(createDocument([sheet], [asset("mask")]));
    }
    for(const dimension of [0, 32, 65, 255, 4097, 8192, NaN, Infinity])
    {
        assert.throws(() => createDocument([{ ...asset("tile"), dimension }], [asset("mask")]));
    }
    const other = { ...asset("tile"), id: "assets/tiles/other.png", label: "other.png", dimension: 512 };
    assert.throws(() => createDocument([asset("tile"), other], [asset("mask")]), /other.png.*256×256/);
    const project = createDocument([asset("tile")], [asset("mask")]);
    project.tileSheets = [...project.tileSheets, other];
    assert.throws(() => validateDocument(project), /other.png.*256×256/);
});

Deno.test("prop snapshots share images, isolate transforms, and account for history memory", () => {
    const project = createDocument([asset("tile")], [asset("mask")]);
    const image: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "blob:prop", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    project.propAssets = [image];
    const emptyBytes = documentBytes(project);
    project.props = [{ id: 0, asset: image, x: 1.2, y: 2.3, rotation: 45 }];
    project.nextPropId = 1;
    validateDocument(project);
    assert(documentBytes(project) > emptyBytes);
    const copy = cloneDocument(project);
    assert(sameContent(project, copy));
    assert.equal(copy.props[0].asset, image);
    copy.props[0].x++;
    assert.equal(project.props[0].x, 1.2);
    assert(!sameContent(project, copy));
    assert(documentAssets(copy).includes(image));
    for(const corrupt of [
        (p: typeof project) => { p.props[0].rotation = 360; },
        (p: typeof project) => { p.props[0].x = NaN; },
        (p: typeof project) => { p.props[0].y = -32.001; },
        (p: typeof project) => { p.props[0].x = 64.001; },
        (p: typeof project) => { p.nextPropId = 0; },
        (p: typeof project) => { p.props.push(p.props[0]); },
        (p: typeof project) => { p.propAssets = []; },
        (p: typeof project) => { p.propAssets = [image, image]; }
    ])
    {
        const candidate = cloneDocument(project);
        corrupt(candidate);
        assert.throws(() => validateDocument(candidate));
    }
});

Deno.test("prop decoding accepts rectangular alpha images and releases temporary bitmaps", async () => {
    const originalBitmap = globalThis.createImageBitmap;
    const originalCanvas = globalThis.OffscreenCanvas;
    let closed = 0;
    let drawn = 0;
    let width = 80;
    globalThis.createImageBitmap = (() => Promise.resolve({ width, height: 20,
        close: () => { closed++; } })) as typeof createImageBitmap;
    globalThis.OffscreenCanvas = class {
        getContext() { return { drawImage: () => { drawn++; } }; }
    } as unknown as typeof OffscreenCanvas;
    try
    {
        const decoded = await decodePropAsset(new Blob(), "stone", "assets/props/stone.png");
        assert.equal(decoded.width, 80);
        assert.equal(decoded.height, 20);
        assert.deepEqual([decoded.pivotX, decoded.pivotY, decoded.scale], [0.5, 0.5, 1]);
        assert.equal(drawn, 1);
        URL.revokeObjectURL(decoded.url);
        width = 4097;
        await assert.rejects(() => decodePropAsset(new Blob(), "stone", "assets/props/stone.png"));
        assert.equal(closed, 2);
    }
    finally
    {
        globalThis.createImageBitmap = originalBitmap;
        globalThis.OffscreenCanvas = originalCanvas;
    }
});

Deno.test("prop metadata validates normalized pivots and finite positive scale", () => {
    const project = createDocument([asset("tile")], [asset("mask")]);
    const image: PropAsset = { kind: "prop", id: "assets/props/a.png", label: "a",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    for(const pivot of [0, 0.5, 1])
    {
        project.propAssets = [{ ...image, pivotX: pivot, pivotY: pivot, scale: 0.25 }];
        validateDocument(project);
    }
    for(const fields of [
        ...["pivotX", "pivotY"].flatMap((key) => [-0.01, 1.01, NaN, Infinity, -Infinity].map((value) => ({ [key]: value }))),
        ...[0, -1, NaN, Infinity, -Infinity].map((scale) => ({ scale }))
    ])
    {
        project.propAssets = [{ ...image, ...fields }];
        assert.throws(() => validateDocument(project));
    }
});

Deno.test("nested tile and prop paths validate, but traversal, mismatched roots and nested masks fail", () => {
    const original = createDocument([asset("tile")], [asset("mask")]);
    const prop: PropAsset = { kind: "prop", id: "assets/props/graveyard/stone.png", label: "graveyard/stone.png",
        blob: new Blob(), url: "", width: 20, height: 40, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    original.propAssets = [prop];
    validateDocument(original);
    for(const path of ["one/two/ground.png", "space #/100%.png", "literal%2F/stone.png"])
    {
        const tile = { ...asset("tile"), id: "assets/tiles/" + path, label: path };
        validateDocument(createDocument([tile], [asset("mask")]));
    }
    for(const path of ["", "/stone.png", "../stone.png", "./stone.png", "one/../stone.png",
        "one//stone.png", "one/", "one\\stone.png", "one/\u0000stone.png"])
    {
        assert.throws(() => createDocument([{ ...asset("tile"), id: "assets/tiles/" + path }], [asset("mask")]));
        const project = cloneDocument(original);
        project.propAssets = [{ ...prop, id: "assets/props/" + path }];
        assert.throws(() => validateDocument(project));
    }
    assert.throws(() => createDocument([{ ...asset("tile"), id: "assets/props/tile.png" }], [asset("mask")]));
    assert.throws(() => createDocument([asset("tile")], [{ ...asset("mask"), id: "assets/masks/nested/mask.png" }]));
    const project = cloneDocument(original);
    project.propAssets = [{ ...prop, id: "assets/tiles/stone.png" }];
    assert.throws(() => validateDocument(project));
});

Deno.test("terrain comparison detects rendering inputs but ignores prop and editor-only changes", () => {
    const original = createDocument([asset("tile")], [asset("mask")]);
    original.layers[1].painted[65] = true;
    assert(sameTerrainContent(original, cloneDocument(original)));
    for(const change of [
        (p: typeof original) => { p.layers[1].painted[65] = false; },
        (p: typeof original) => { p.layers[1].sheet = { ...p.layers[1].sheet }; },
        (p: typeof original) => { p.layers.reverse(); },
        (p: typeof original) => { p.layers.pop(); },
        (p: typeof original) => { p.maskMode = 0; },
        (p: typeof original) => { p.masks = [{ ...p.masks[0] }]; },
        (p: typeof original) => { p.masks = [...p.masks, { ...p.masks[0] }]; }
    ])
    {
        const changed = cloneDocument(original);
        change(changed);
        assert(!sameTerrainContent(original, changed));
        assert(!sameTerrainContent(changed, original));
    }
    const prop: PropAsset = { kind: "prop", id: "assets/props/stone.png", label: "stone.png",
        blob: new Blob(), url: "", width: 20, height: 40, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    const changed = cloneDocument(original);
    changed.propAssets = [prop];
    changed.props = [{ id: 0, asset: prop, x: 1, y: 2, rotation: 90 }];
    changed.nextPropId = 1;
    changed.view.zoom = 0.5;
    changed.activeLayerId = 0;
    changed.layers[1] = { ...changed.layers[1], id: 42 };
    changed.nextLayerId = 43;
    changed.tileSheets = [...changed.tileSheets, { ...changed.tileSheets[0] }];
    assert(sameTerrainContent(original, changed));
    // Mask order participates in procedural selection even when occupancy stays the same.
    original.masks = [original.masks[0], { ...original.masks[0] }];
    const reordered = cloneDocument(original);
    reordered.masks = [...reordered.masks].reverse();
    assert(!sameTerrainContent(original, reordered));
});

Deno.test("map size controls occupancy lengths and prop bounds, and participates in equality", () => {
    const original = createDocument([asset("tile")], [asset("mask")]);
    for(const size of [2, 4, 8, 16, 32, 64, 128, 256, 512])
    {
        const project = cloneDocument(original);
        project.gridSize = size;
        project.layers = project.layers.map((layer, index) => ({ ...layer, painted: Array(size * size).fill(index === 0) }));
        validateDocument(project);
        assert.equal(sameContent(project, original), size === 64);
        assert.equal(sameContent(original, project), size === 64);
        assert.equal(sameTerrainContent(project, original), size === 64);
        const image: PropAsset = { kind: "prop", id: "assets/props/a.png", label: "a", blob: new Blob(),
            url: "", width: 10, height: 10, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
        project.propAssets = [image];
        project.props = [{ id: 0, asset: image, x: size / 2, y: -size / 2, rotation: 0 }];
        project.nextPropId = 1;
        validateDocument(project);
        project.props[0].x += 0.001;
        assert.throws(() => validateDocument(project), /prop/);
    }
    for(const size of [0, 1, 3, 6, 511, 513, -1, NaN, 2.5, Infinity])
    {
        assert.throws(() => validateDocument({ ...original, gridSize: size }), /dimensions/);
    }
    const smaller = cloneDocument(original);
    smaller.gridSize = 2;
    assert.throws(() => validateDocument(smaller), /painted-cell grid/);
});

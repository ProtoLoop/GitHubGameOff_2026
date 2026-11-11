import { strict as assert } from "node:assert";
import { cellMasks } from "./placement.ts";
import { atlasGridSize, cloneDocument, createDocument, sameContent, validateDocument, type ImageAsset } from "./document.ts";

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
        (p: typeof original) => { p.tileSize = 63; },
        (p: typeof original) => { p.masks = []; }
    ])
    {
        const project = cloneDocument(original);
        corrupt(project);
        assert.throws(() => validateDocument(project));
    }
    assert.equal(atlasGridSize(256, 64), 4);
    assert.equal(atlasGridSize(192, 64), 3);
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

Deno.test("terrain atlases share dimensions and support any integral square grid including 1×1", () => {
    for(const [dimension, size, expected] of [[64, 64, 1], [128, 64, 2], [192, 64, 3],
        [4096, 4096, 1], [4096, 128, 32], [4096, 64, 64]])
    {
        const sheet = { ...asset("tile"), dimension };
        const project = createDocument([sheet], [asset("mask")]);
        assert.equal(project.tileSize, dimension);
        project.tileSize = size;
        assert.equal(atlasGridSize(dimension, size), expected);
        validateDocument(project);
    }
    for(const [dimension, size] of [[4096, 192], [256, 32], [256, 0], [256, 512],
        [256, 65], [8192, 64], [255, 64], [256, NaN], [NaN, 64]])
    {
        assert.equal(atlasGridSize(dimension, size), 0);
    }
    const other = { ...asset("tile"), id: "assets/tiles/other.png", label: "other.png", dimension: 512 };
    assert.throws(() => createDocument([asset("tile"), other], [asset("mask")]), /other.png.*256×256/);
    const project = createDocument([asset("tile")], [asset("mask")]);
    project.tileSheets = [...project.tileSheets, other];
    assert.throws(() => validateDocument(project), /other.png.*256×256/);
});

import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset, type Rotation } from "./document.ts";
import { automaticTile, cellMasks, maskSource, sampleMask } from "./placement.ts";

function asset(kind: "tile" | "mask", name: string): ImageAsset
{
    return { id: `assets/${kind === "tile" ? "tiles" : "masks"}/${name}.png`, kind, label: name,
        blob: new Blob(), dimension: 256, pixels: new Uint8ClampedArray(), coverage: new Uint8Array(65536), url: "" };
}

Deno.test("painting affects adjoining masks while unpainted cells still have texture", () => {
    const project = createDocument([asset("tile", "grass")], [asset("mask", "alpha")]);
    const layer = project.layers[1];
    const before = automaticTile(project, layer, 64);
    layer.painted[65] = true;
    assert.deepEqual(cellMasks(project, layer, 65).map((mask) => [mask!.cell, mask!.tile]),
        [[66, 8], [67, 4], [131, 2], [132, 1]]);
    assert.equal(layer.painted[64], false);
    assert.deepEqual(cellMasks(project, layer, 64)[1], cellMasks(project, layer, 65)[0]);
    assert.deepEqual(automaticTile(project, layer, 64), before);
    assert.equal(project.layers[0].painted.filter(Boolean).length, 4096);
    layer.painted[65] = false;
    assert(cellMasks(project, layer, 65).every((mask) => mask === null));
});

Deno.test("placement orientations preserve all patterns and perimeter masks", () => {
    const project = createDocument([asset("tile", "grass")], [asset("mask", "alpha")]);
    const layer = project.layers[1];
    const seen = new Set<string>();
    for(let pattern = 1; pattern < 16; pattern++)
    {
        for(let y = 1; y < 63; y++)
        {
            layer.painted.fill(false);
            const cells = [(y - 1) * 64, (y - 1) * 64 + 1, y * 64, y * 64 + 1];
            cells.forEach((cell, bit) => { if(pattern & (1 << bit)) { layer.painted[cell] = true; } });
            const mask = cellMasks(project, layer, y * 64 + 1)[0]!;
            assert.equal(mask.tile, pattern);
            seen.add(`${mask.rotation},${mask.hflip},${mask.vflip}`);
            for(let bit = 0; bit < 4; bit++)
            {
                const source = maskSource(mask, bit % 2, Math.floor(bit / 2), 1);
                assert.equal((pattern >>> (source.y * 2 + source.x)) & 1, (pattern >>> bit) & 1);
            }
        }
    }
    assert.equal(seen.size, 8);
    for(let cell = 0; cell < 4096; cell++) { layer.painted[cell] = true; }
    assert.deepEqual(cellMasks(project, layer, 0).map((mask) => mask!.tile), [8, 12, 10, 15]);
    assert.deepEqual(cellMasks(project, layer, 4095).map((mask) => mask!.tile), [15, 5, 3, 1]);
});

Deno.test("clockwise rotation follows source-axis flips for every combination", () => {
    const rotations: Rotation[] = ["0", "90", "180", "270"];
    for(const rotation of rotations)
    {
        for(const hflip of [false, true])
        {
            for(const vflip of [false, true])
            {
                // Independently move a source point forward: flips, then rotation.
                let x = hflip ? 63 - 9 : 9;
                let y = vflip ? 63 - 22 : 22;
                for(let turn = 0; turn < Number(rotation) / 90; turn++) { [x, y] = [63 - y, x]; }
                assert.deepEqual(maskSource({ rotation, hflip, vflip }, x, y, 63), { x: 9, y: 22 });
            }
        }
    }
    const mask = asset("mask", "alpha");
    mask.coverage[8 * 4096 + 22 * 64 + 9] = 173;
    assert.equal(sampleMask({ sheet: 0, cell: 66, tile: 8, rotation: "90", hflip: true, vflip: false },
        [mask], 41, 54), 173);
    assert.equal(sampleMask(null, [mask], 0, 0), 0);
});

Deno.test("selection follows layer order rather than session IDs", () => {
    const project = createDocument([asset("tile", "grass")], [asset("mask", "a"), asset("mask", "b")]);
    project.layers[1] = { ...project.layers[1], id: 42 };
    const layer = project.layers[1];
    layer.painted[65] = true;
    const original = cellMasks(project, layer, 65);
    const tiles = Array.from({ length: 4096 }, (_, cell) => automaticTile(project, layer, cell));
    assert.equal(new Set(tiles.map((tile) => tile.tile)).size, 16);
    assert.equal(new Set(tiles.map((tile) => `${tile.hflip},${tile.vflip}`)).size, 4);
    layer.painted[65] = false;
    layer.painted[65] = true;
    layer.painted[4095] = true;
    project.layers.push({ ...layer, id: 2, painted: Array(4096).fill(false) });
    assert.deepEqual(cellMasks(project, layer, 65), original);
    assert.deepEqual(Array.from({ length: 4096 }, (_, cell) => automaticTile(project, layer, cell)), tiles);
    [project.layers[1], project.layers[2]] = [project.layers[2], project.layers[1]];
    const reorderedTiles = Array.from({ length: 4096 }, (_, cell) => automaticTile(project, layer, cell));
    assert.notDeepEqual(reorderedTiles, tiles);
    assert.notDeepEqual(cellMasks(project, layer, 65), original);
    [project.layers[1], project.layers[2]] = [project.layers[2], project.layers[1]];
    assert.deepEqual(cellMasks(project, layer, 65), original);
    project.maskMode = 1;
    assert(cellMasks(project, layer, 65).every((mask) => mask!.sheet === 1));
    assert.deepEqual(Array.from({ length: 4096 }, (_, cell) => automaticTile(project, layer, cell)), tiles);
});

Deno.test("diagonal coverage is sampled in all four cells, including unpainted neighbors", () => {
    const mask = asset("mask", "alpha");
    // A fully covered center crossing all four quadrants of diagonal pattern 9.
    for(let y = 28; y < 36; y++)
    {
        for(let x = 28; x < 36; x++) { mask.coverage[9 * 4096 + y * 64 + x] = 255; }
    }
    const project = createDocument([asset("tile", "grass")], [mask]);
    const layer = project.layers[1];
    layer.painted[66] = true;
    layer.painted[131] = true;
    for(const [cell, quadrant, x, y] of [[66, 3, 31, 31], [67, 2, 32, 31], [130, 1, 31, 32], [131, 0, 32, 32]])
    {
        const junction = cellMasks(project, layer, cell)[quadrant];
        assert.equal(junction!.tile, 9);
        assert.equal(sampleMask(junction, project.masks, x, y), 255);
    }
    assert.equal(layer.painted[67], false);
    assert.equal(layer.painted[130], false);
});

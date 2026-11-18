import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset } from "./document.ts";
import { drawTerrainRegion } from "./renderer.ts";

function material(name: string, dimension: number, channel: number): ImageAsset
{
    const pixels = new Uint8ClampedArray(dimension * dimension * 4);
    for(let y = 0; y < dimension; y++)
    {
        for(let x = 0; x < dimension; x++)
        {
            pixels[(y * dimension + x) * 4 + channel] = (x + y * 3) % 256;
        }
    }
    return { id: `assets/tiles/${name}.png`, kind: "tile", label: name, blob: new Blob(),
        dimension, pixels, coverage: new Uint8Array(), url: "" };
}

Deno.test("renderer repeats whole textures and partial redraws preserve compositing coordinates", () => {
    // 192 does not divide the map width. The regions include repeat boundaries and the partial final repeat.
    const base = material("base", 192, 0);
    const overlay = material("overlay", 192, 1);
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask",
        coverage: new Uint8Array(65536).fill(128) };
    const project = createDocument([base, overlay], [mask]);
    project.layers[1].painted.fill(true);
    const draws: { data: Uint8ClampedArray; width: number; height: number; x: number; y: number }[] = [];
    const context = {
        createImageData: (width: number, height: number) => ({ width, height, data: new Uint8ClampedArray(width * height * 4) }),
        putImageData: (image: { data: Uint8ClampedArray; width: number; height: number }, x: number, y: number) => {
            draws.push({ ...image, x, y });
        }
    };
    const canvas = { width: 4096, height: 4096, getContext: () => context } as unknown as HTMLCanvasElement;
    drawTerrainRegion(project, canvas, 2, 2, 5, 5);
    drawTerrainRegion(project, canvas, 3, 3, 4, 4);
    drawTerrainRegion(project, canvas, 63, 63, 64, 64);
    for(const draw of draws)
    {
        for(let y = 0; y < draw.height; y++)
        {
            for(let x = 0; x < draw.width; x++)
            {
                const value = ((draw.x + x) % 192 + ((draw.y + y) % 192) * 3) % 256;
                const offset = (y * draw.width + x) * 4;
                assert.deepEqual(Array.from(draw.data.subarray(offset, offset + 4)),
                    [Math.round(value * 127 / 255), Math.round(value * 128 / 255), 0, 255]);
            }
        }
    }
    const region = draws[0];
    const partial = draws[1];
    for(let y = 0; y < 64; y++)
    {
        const start = ((y + 64) * region.width + 64) * 4;
        assert.deepEqual(partial.data.subarray(y * 64 * 4, (y + 1) * 64 * 4),
            region.data.subarray(start, start + 64 * 4));
    }
});

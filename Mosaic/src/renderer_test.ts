import { strict as assert } from "node:assert";
import { createDocument, cloneDocument, sameContent, type ImageAsset, type PropAsset } from "./document.ts";
import { drawTerrainRegion, drawProps } from "./renderer.ts";

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

Deno.test("prop overlay clears stale positions, draws in order and transforms around image centers", () => {
    const image: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.propAssets = [image];
    project.props = [{ id: 0, asset: image, x: 100, y: 200, rotation: 90 },
        { id: 1, asset: image, x: 300, y: 400, rotation: 0 }];
    project.nextPropId = 2;
    const calls: unknown[][] = [];
    const context = {
        clearRect: (...args: number[]) => calls.push(["clear", ...args]),
        save: () => calls.push(["save"]), restore: () => calls.push(["restore"]),
        translate: (...args: number[]) => calls.push(["translate", ...args]),
        rotate: (angle: number) => calls.push(["rotate", angle]),
        drawImage: (...args: unknown[]) => calls.push(["image", ...args]),
        strokeRect: (...args: number[]) => calls.push(["selection", ...args])
    };
    const canvas = { width: 4096, height: 4096, getContext: () => context } as unknown as HTMLCanvasElement;
    drawProps(project, canvas, new Set([0]), null, null);
    assert.deepEqual(calls.slice(0, 6), [["clear", 0, 0, 4096, 4096], ["save"],
        ["translate", 100, 200], ["rotate", Math.PI / 2], ["image", image.image, -40, -10], ["restore"]]);
    assert.deepEqual(calls[7], ["translate", 300, 400]);
    assert.deepEqual(calls.at(-2), ["selection", -40, -10, 80, 20]);
    calls.length = 0;
    project.props = [];
    drawProps(project, canvas, new Set(), null, null);
    assert.deepEqual(calls, [["clear", 0, 0, 4096, 4096]]);
});

Deno.test("preview and multi-selection render independently of document state and clear on the next draw", () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 100, y: 100, rotation: 0 }, { id: 1, asset, x: 200, y: 200, rotation: 45 }];
    project.nextPropId = 2;
    const snapshot = cloneDocument(project);
    const calls: unknown[][] = [];
    const alphas: number[] = [];
    const context = {
        globalAlpha: 1,
        clearRect: (...args: number[]) => calls.push(["clear", ...args]),
        save() { alphas.push(this.globalAlpha); },
        restore() { this.globalAlpha = alphas.pop()!; },
        translate: () => {}, rotate: () => {},
        drawImage(...args: unknown[]) { calls.push(["image", this.globalAlpha, ...args]); },
        strokeRect: (...args: number[]) => calls.push(["stroke", ...args]),
        fillRect: (...args: number[]) => calls.push(["fill", ...args])
    };
    const canvas = { width: 4096, height: 4096, getContext: () => context } as unknown as HTMLCanvasElement;
    drawProps(project, canvas, new Set([0, 1]), { asset, x: 400, y: 500 },
        { start: { x: 300, y: 400 }, end: { x: 50, y: 60 } });
    assert.equal(calls.filter((call) => call[0] === "stroke").length, 3);
    assert(calls.some((call) => JSON.stringify(call) === JSON.stringify(["fill", 50, 60, 250, 340])));
    assert.deepEqual(calls.filter((call) => call[0] === "image").at(-1), ["image", 0.5, asset.image, 360, 490]);
    assert.equal(context.globalAlpha, 1);
    assert(sameContent(project, snapshot));
    calls.length = 0;
    drawProps(project, canvas, new Set(), null, null);
    assert.deepEqual(calls[0], ["clear", 0, 0, 4096, 4096]);
    assert.equal(calls.filter((call) => call[0] === "image").length, 2);
    assert.equal(calls.filter((call) => call[0] === "stroke" || call[0] === "fill").length, 0);
});

Deno.test("terrain preview keeps thumbnail screen size across zoom levels without changing the document", () => {
    const terrain = material("preview", 192, 1);
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([terrain], [mask]);
    const snapshot = cloneDocument(project);
    const originalCanvas = globalThis.OffscreenCanvas;
    const decoded: { width: number; height: number; pixels: Uint8ClampedArray }[] = [];
    globalThis.OffscreenCanvas = class {
        constructor(readonly width: number, readonly height: number) {}
        getContext() {
            return {
                createImageData: (width: number, height: number) => ({ data: new Uint8ClampedArray(width * height * 4) }),
                putImageData: (image: { data: Uint8ClampedArray }) => {
                    decoded.push({ width: this.width, height: this.height, pixels: image.data });
                }
            };
        }
    } as unknown as typeof OffscreenCanvas;
    const calls: { image: CanvasImageSource; x: number; y: number; width: number; height: number; alpha: number }[] = [];
    let clears = 0;
    const savedAlpha: number[] = [];
    const context = {
        globalAlpha: 1,
        clearRect: () => { clears++; },
        save() { savedAlpha.push(this.globalAlpha); },
        restore() { this.globalAlpha = savedAlpha.pop()!; },
        drawImage(image: CanvasImageSource, x: number, y: number, width: number, height: number) { calls.push({ image, x, y, width, height, alpha: this.globalAlpha }); }
    };
    const canvas = { width: 4096, height: 4096, getContext: () => context } as unknown as HTMLCanvasElement;
    try
    {
        for(const thumbnailSize of [56, 42])
        {
            for(const zoom of [1 / 16, 0.25, 1, 4])
            {
                project.view.zoom = zoom;
                drawProps(project, canvas, new Set(), { asset: terrain, x: 400, y: 500,
                    size: { width: thumbnailSize / zoom, height: thumbnailSize / zoom } }, null);
                const call = calls.at(-1)!;
                assert.equal(call.width * zoom, thumbnailSize);
                assert.equal(call.height * zoom, thumbnailSize);
                assert.equal(call.x + call.width / 2, 400);
                assert.equal(call.y + call.height / 2, 500);
                assert.equal(call.alpha, 0.5);
                assert.equal(call.image, calls[0].image);
                assert.equal(context.globalAlpha, 1);
            }
        }
        assert.equal(decoded.length, 1);
        assert.equal(decoded[0].width, 192);
        assert.equal(decoded[0].height, 192);
        assert.equal(decoded[0].pixels[4 + 1], terrain.pixels[4 + 1]);
        assert.equal(decoded[0].pixels[3], 255);
        assert.equal(terrain.pixels[3], 0);
        drawProps(project, canvas, new Set(), null, null);
        assert.equal(calls.length, 8);
        assert.equal(clears, 9);
        assert(sameContent(project, snapshot));
    }
    finally { globalThis.OffscreenCanvas = originalCanvas; }
});

import { strict as assert } from "node:assert";
import { createDocument, cloneDocument, sameContent, type ImageAsset, type PropAsset } from "./document.ts";
import { drawTerrainRegion, drawCoverageRegion, drawProps, type RenderView } from "./renderer.ts";

const fullView: RenderView = { x: 0, y: 0, width: 4096, height: 4096, zoom: 1 };

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
        setTransform: () => {},
        createImageData: (width: number, height: number) => ({ width, height, data: new Uint8ClampedArray(width * height * 4) }),
        putImageData: (image: { data: Uint8ClampedArray; width: number; height: number }, x: number, y: number) => {
            draws.push({ ...image, x, y });
        }
    };
    const canvas = { width: 4096, height: 4096, getContext: () => context } as unknown as HTMLCanvasElement;
    drawTerrainRegion(project, canvas, fullView, 2, 2, 5, 5);
    drawTerrainRegion(project, canvas, fullView, 3, 3, 4, 4);
    drawTerrainRegion(project, canvas, fullView, 63, 63, 64, 64);
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
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.propAssets = [image];
    project.props = [{ id: 0, asset: image, x: 100 / 64 - 32, y: 32 - 200 / 64, rotation: 90 },
        { id: 1, asset: image, x: 300 / 64 - 32, y: 32 - 400 / 64, rotation: 0 }];
    project.nextPropId = 2;
    const calls: unknown[][] = [];
    const context = {
        setTransform: () => {},
        clearRect: (...args: number[]) => calls.push(["clear", ...args]),
        save: () => calls.push(["save"]), restore: () => calls.push(["restore"]),
        translate: (...args: number[]) => calls.push(["translate", ...args]),
        rotate: (angle: number) => calls.push(["rotate", angle]),
        drawImage: (...args: unknown[]) => calls.push(["image", ...args]),
        strokeRect: (...args: number[]) => calls.push(["selection", ...args]),
        fillRect: (...args: number[]) => calls.push(["pivot", ...args])
    };
    const canvas = { width: 4096, height: 4096, getContext: () => context } as unknown as HTMLCanvasElement;
    drawProps(project, canvas, fullView, new Set([0]), null, null);
    assert.deepEqual(calls.slice(0, 7), [["clear", 0, 0, 4096, 4096], ["save"], ["save"],
        ["translate", 100, 200], ["rotate", Math.PI / 2], ["image", image.image, -40, -10, 80, 20], ["restore"]]);
    assert.deepEqual(calls[8], ["translate", 300, 400]);
    assert.deepEqual(calls.filter((call) => call[0] === "selection"), [["selection", -40, -10, 80, 20]]);
    assert.equal(calls.filter((call) => call[0] === "pivot").length, 4);
    calls.length = 0;
    project.props = [];
    drawProps(project, canvas, fullView, new Set(), null, null);
    assert.deepEqual(calls, [["clear", 0, 0, 4096, 4096], ["save"], ["restore"]]);
});

Deno.test("preview and multi-selection render independently of document state and clear on the next draw", () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.propAssets = [asset];
    project.props = [{ id: 0, asset, x: 100 / 64 - 32, y: 32 - 100 / 64, rotation: 0 }, { id: 1, asset, x: 200 / 64 - 32, y: 32 - 200 / 64, rotation: 45 }];
    project.nextPropId = 2;
    const snapshot = cloneDocument(project);
    const calls: unknown[][] = [];
    const alphas: number[] = [];
    const context = {
        setTransform: () => {},
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
    drawProps(project, canvas, fullView, new Set([0, 1]), { asset, x: 400 / 64 - 32, y: 32 - 500 / 64 },
        { start: { x: 300 / 64 - 32, y: 32 - 400 / 64 }, end: { x: 50 / 64 - 32, y: 32 - 60 / 64 } });
    assert.equal(calls.filter((call) => call[0] === "stroke").length, 3);
    assert(calls.some((call) => JSON.stringify(call) === JSON.stringify(["fill", 50, 60, 250, 340])));
    assert.deepEqual(calls.filter((call) => call[0] === "image").at(-1), ["image", 0.5, asset.image, 360, 490, 80, 20]);
    assert.equal(context.globalAlpha, 1);
    assert(sameContent(project, snapshot));
    calls.length = 0;
    drawProps(project, canvas, fullView, new Set(), null, null);
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
        setTransform: () => {},
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
                drawProps(project, canvas, fullView, new Set(), { asset: terrain, x: 400 / 64 - 32, y: 32 - 500 / 64,
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
        drawProps(project, canvas, fullView, new Set(), null, null);
        assert.equal(calls.length, 8);
        assert.equal(clears, 9);
        assert(sameContent(project, snapshot));
    }
    finally { globalThis.OffscreenCanvas = originalCanvas; }
});

Deno.test("viewport rendering on a 512-cell map bounds allocation and preserves world sampling and dirty regions", () => {
    const base = material("base", 192, 0);
    const overlay = material("overlay", 192, 1);
    const mask: ImageAsset = { ...material("mask", 256, 0), kind: "mask", id: "assets/masks/mask.png",
        coverage: new Uint8Array(65536).fill(128) };
    const project = createDocument([base, overlay], [mask]);
    project.gridSize = 512;
    project.layers = project.layers.map((layer) => ({ ...layer, painted: Array(512 * 512).fill(true) }));
    const draws: { data: Uint8ClampedArray; width: number; height: number; x: number; y: number }[] = [];
    const context = {
        createImageData: (width: number, height: number) => {
            assert(width <= 173 && height <= 109);
            return { width, height, data: new Uint8ClampedArray(width * height * 4) };
        },
        putImageData: (image: { data: Uint8ClampedArray; width: number; height: number }, x: number, y: number) => {
            draws.push({ ...image, x, y });
        }
    };
    const canvas = { width: 0, height: 0, getContext: () => context } as unknown as HTMLCanvasElement;
    for(const zoom of [1 / 16, 0.37, 1, 2.3, 4])
    {
        const view = { x: 30000.25, y: 29000.75, width: 173, height: 109, zoom };
        drawTerrainRegion(project, canvas, view, 0, 0, 512, 512);
        assert.equal(canvas.width, view.width);
        assert.equal(canvas.height, view.height);
        const full = draws.at(-1)!;
        for(let y = 0; y < full.height; y++)
        {
            for(let x = 0; x < full.width; x++)
            {
                const worldX = Math.floor(view.x + (x + 0.5) / zoom);
                const worldY = Math.floor(view.y + (y + 0.5) / zoom);
                const value = (worldX % 192 + (worldY % 192) * 3) % 256;
                const offset = (y * full.width + x) * 4;
                assert.deepEqual(Array.from(full.data.subarray(offset, offset + 4)),
                    [Math.round(value * 127 / 255), Math.round(value * 128 / 255), 0, 255]);
            }
        }
        drawTerrainRegion(project, canvas, view, 469, 454, 470, 455);
        const partial = draws.at(-1)!;
        for(let y = 0; y < partial.height; y++)
        {
            const start = ((y + partial.y) * full.width + partial.x) * 4;
            assert.deepEqual(partial.data.subarray(y * partial.width * 4, (y + 1) * partial.width * 4),
                full.data.subarray(start, start + partial.width * 4));
        }
        const count = draws.length;
        drawTerrainRegion(project, canvas, view, 0, 0, 1, 1);
        assert.equal(draws.length, count);
        drawCoverageRegion(project, canvas, view, 0, 0, 512, 512);
        assert(draws.at(-1)!.data.every((value, index) => value === (index % 4 === 3 ? 128 : 255)));
    }
    // The full-map overview is sampled at panel size, without an intermediate world-sized image.
    drawCoverageRegion(project, canvas, { x: 0, y: 0, width: 100, height: 100, zoom: 100 / 32768 }, 0, 0, 512, 512);
    assert.equal(canvas.width, 100);
    assert.equal(canvas.height, 100);
});

Deno.test("viewport props cull rotated bounds rather than centers and use a shared map transform", () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 0.5, pivotY: 0.5, scale: 1 };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.props = [{ id: 0, asset, x: 110 / 64 - 32, y: 32 - 80 / 64, rotation: 90 },
        { id: 1, asset, x: 500 / 64 - 32, y: 32 - 500 / 64, rotation: 0 }];
    const calls: unknown[][] = [];
    const context = {
        save: () => {}, restore: () => {}, clearRect: () => {}, rotate: () => {},
        setTransform: (...args: number[]) => calls.push(["transform", ...args]),
        translate: (...args: number[]) => calls.push(["translate", ...args]),
        drawImage: () => calls.push(["image"]), strokeRect: () => calls.push(["selection"])
    };
    const canvas = { width: 0, height: 0, getContext: () => context } as unknown as HTMLCanvasElement;
    drawProps(project, canvas, { x: 100, y: 100, width: 150, height: 120, zoom: 2 }, new Set([0, 1]), null, null);
    assert.deepEqual(calls[0], ["transform", 2, 0, 0, 2, -200, -200]);
    assert.equal(calls.filter((call) => call[0] === "image").length, 1);
    assert.equal(calls.filter((call) => call[0] === "selection").length, 1);
    assert.equal(canvas.width, 150);
    assert.equal(canvas.height, 120);
});

Deno.test("scaled props rotate about their pivots and retain visible artwork when the pivot is offscreen", () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 0, pivotY: 1, scale: 2 };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.propAssets = [asset];
    // The fixed pivot is at map pixels (100, 100). After rotation the image covers [100, 140] × [100, 260].
    project.props = [{ id: 0, asset, x: 100 / 64 - 32, y: 32 - 100 / 64, rotation: 90 }];
    project.nextPropId = 1;
    const calls: unknown[][] = [];
    const context = {
        save: () => {}, restore: () => {}, clearRect: () => {}, setTransform: () => {},
        translate: (...args: number[]) => calls.push(["translate", ...args]),
        rotate: (angle: number) => calls.push(["rotate", angle]),
        drawImage: (...args: unknown[]) => calls.push(["image", ...args]),
        strokeRect: (...args: number[]) => calls.push(["selection", ...args]),
        fillRect: (...args: number[]) => calls.push(["pivot", ...args])
    };
    const canvas = { width: 0, height: 0, getContext: () => context } as unknown as HTMLCanvasElement;
    const view = { x: 110, y: 200, width: 20, height: 20, zoom: 1 };
    drawProps(project, canvas, view, new Set([0]), null, null);
    assert.deepEqual(calls, [
        ["translate", 100, 100], ["rotate", Math.PI / 2], ["image", asset.image, -0, -40, 160, 40],
        ["translate", 100, 100], ["rotate", Math.PI / 2], ["selection", -0, -40, 160, 40]
    ]);
    calls.length = 0;
    drawProps(project, canvas, { ...view, x: 150 }, new Set([0]), null, null);
    assert.deepEqual(calls, []);
    project.props = [];
    drawProps(project, canvas, fullView, new Set(), { asset, x: 100 / 64 - 32, y: 32 - 100 / 64 }, null);
    assert.deepEqual(calls, [["image", asset.image, 100, 60, 160, 40]]);
});

Deno.test("selected pivot markers stay upright at fixed screen size and clear on deselection", () => {
    const asset: PropAsset = { id: "assets/props/stone.png", kind: "prop", label: "stone.png",
        blob: new Blob(), url: "", width: 80, height: 20, image: {} as CanvasImageSource,
        pivotX: 1, pivotY: 1, scale: 2 };
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask" };
    const project = createDocument([material("base", 64, 0)], [mask]);
    project.propAssets = [asset];
    project.props = [0, 1, 2].map((id) => ({ id, asset,
        x: (100 + id * 50) / 64 - 32, y: 32 - (200 + id * 20) / 64, rotation: id * 45 }));
    project.nextPropId = 3;
    const before = cloneDocument(project);
    const fills: { rect: number[]; color: string; transform: number[] }[] = [];
    let transform: number[] = [];
    let clears = 0;
    const context = {
        fillStyle: "",
        save: () => {}, restore: () => {}, translate: () => {}, rotate: () => {},
        clearRect: () => { clears++; }, drawImage: () => {}, strokeRect: () => {},
        setTransform: (...args: number[]) => { transform = args; },
        fillRect(...rect: number[]) { fills.push({ rect, color: this.fillStyle, transform }); }
    };
    const canvas = { width: 0, height: 0, getContext: () => context } as unknown as HTMLCanvasElement;
    for(const zoom of [1 / 16, 0.25, 1, 4])
    {
        fills.length = 0;
        drawProps(project, canvas, { x: 80, y: 160, width: 400, height: 400, zoom }, new Set([0, 1]), null, null);
        assert.equal(fills.length, 8);
        for(const id of [0, 1])
        {
            const x = (20 + id * 50) * zoom, y = (40 + id * 20) * zoom;
            const marker = fills.slice(id * 4, id * 4 + 4);
            assert.deepEqual(marker.map((fill) => fill.rect), [
                [x - 7, y - 2, 14, 4], [x - 2, y - 7, 4, 14],
                [x - 6, y - 1, 12, 2], [x - 1, y - 6, 2, 12]
            ]);
            assert.deepEqual(marker.map((fill) => fill.color), ["#161b16", "#161b16", "#ff00ff", "#ff00ff"]);
            assert(marker.every((fill) => JSON.stringify(fill.transform) === "[1,0,0,1,0,0]"));
        }
    }
    assert(sameContent(project, before));
    fills.length = 0;
    // A marker remains partially visible even when the image bounds are outside the viewport.
    drawProps(project, canvas, { x: 106, y: 190, width: 100, height: 100, zoom: 1 }, new Set([0]), null, null);
    assert.equal(fills.length, 4);
    fills.length = 0;
    drawProps(project, canvas, { x: 108, y: 190, width: 100, height: 100, zoom: 1 }, new Set([0]), null, null);
    assert.equal(fills.length, 0);
    drawProps(project, canvas, fullView, new Set(), null, null);
    assert.equal(fills.length, 0);
    assert.equal(clears, 7);
});

Deno.test("fractional viewport sampling matches world-resolution masks at map edges and after canvas resize", () => {
    const mask: ImageAsset = { ...material("mask", 256, 0), id: "assets/masks/mask.png", kind: "mask",
        coverage: Uint8Array.from({ length: 65536 }, (_, index) => (index * 17 + Math.floor(index / 64)) % 256) };
    const project = createDocument([material("base", 192, 0), material("overlay", 192, 1)], [mask]);
    project.gridSize = 2;
    project.layers = project.layers.map((layer, index) => ({ ...layer,
        painted: index === 0 ? [true, true, true, true] : [true, false, false, true] }));
    let pixels: Uint8ClampedArray = new Uint8ClampedArray();
    let dimensions: number[] = [];
    const context = {
        createImageData: (width: number, height: number) => ({ width, height, data: new Uint8ClampedArray(width * height * 4) }),
        putImageData: (image: { data: Uint8ClampedArray; width: number; height: number }, x: number, y: number) => {
            pixels = image.data;
            dimensions = [image.width, image.height, x, y];
        }
    };
    const canvas = { width: 0, height: 0, getContext: () => context } as unknown as HTMLCanvasElement;
    for(const draw of [drawTerrainRegion, drawCoverageRegion])
    {
        draw(project, canvas, { x: 0, y: 0, width: 128, height: 128, zoom: 1 }, 0, 0, 2, 2);
        const reference = pixels;
        const view = { x: 61.25, y: 59.75, width: 30, height: 30, zoom: 0.47 };
        // Resizing the canvas forces all visible pixels to be restored, even for a small dirty region.
        draw(project, canvas, view, 0, 0, 1, 1);
        assert.deepEqual(dimensions, [30, 30, 0, 0]);
        for(let y = 0; y < 30; y++)
        {
            for(let x = 0; x < 30; x++)
            {
                const sourceX = Math.floor(view.x + (x + 0.5) / view.zoom);
                const sourceY = Math.floor(view.y + (y + 0.5) / view.zoom);
                const source = (sourceY * 128 + sourceX) * 4;
                const target = (y * 30 + x) * 4;
                assert.deepEqual(pixels.subarray(target, target + 4), reference.subarray(source, source + 4));
            }
        }
    }
});

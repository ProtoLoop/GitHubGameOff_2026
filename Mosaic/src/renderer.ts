import { type ProjectDocument, type MaskPlacement } from "./document.ts";
import { automaticTile, cellMasks, sampleMask, terrainSourceOffset } from "./placement.ts";
import { kGridSize, kCellSize, kHalfCellSize } from "./grid.ts";

/// Rendering consumes a validated document and integer cell bounds.
function prepareCanvas(canvas: HTMLCanvasElement): CanvasRenderingContext2D
{
    const size = kGridSize * kCellSize;
    if(canvas.width !== size || canvas.height !== size)
    {
        canvas.width = size;
        canvas.height = size;
    }
    const context = canvas.getContext("2d");
    if(context === null)
    {
        throw new Error("Canvas 2D is unavailable");
    }
    return context;
}

function cellCoverage(project: ProjectDocument, masks: readonly (MaskPlacement | null)[], x: number, y: number): number
{
    const quadrant = (y >= kHalfCellSize ? 2 : 0) + (x >= kHalfCellSize ? 1 : 0);
    return sampleMask(masks[quadrant], project.masks,
        (x + kHalfCellSize) % kCellSize, (y + kHalfCellSize) % kCellSize);
}

/// Render a rectangle of whole cells. Painting includes the one-cell border
/// touched by the four junction masks whose patterns may have changed.
/// Other pixels remain untouched.
export function drawTerrainRegion(project: ProjectDocument, canvas: HTMLCanvasElement, startX: number, startY: number, endX: number, endY: number): void
{
    const context = prepareCanvas(canvas);
    const width = (endX - startX) * kCellSize;
    const height = (endY - startY) * kCellSize;
    const image = context.createImageData(width, height);
    const base = project.layers[0];
    const rowBytes = base.sheet.dimension * 4;
    const overlays = project.layers.slice(1);
    for(let tileY = startY; tileY < endY; tileY++)
    {
        for(let tileX = startX; tileX < endX; tileX++)
        {
            const cell = tileY * kGridSize + tileX;
            // Equal atlas dimensions and a shared tile size give every layer the same source offset.
            const tile = automaticTile(project, base, cell);
            const start = terrainSourceOffset(project, base, tile, 0, 0);
            const layers = overlays.map((layer) => {
                return { pixels: layer.sheet.pixels, masks: cellMasks(project, layer, cell) };
            });
            for(let y = 0; y < kCellSize; y++)
            {
                for(let x = 0; x < kCellSize; x++)
                {
                    const source = start + y * rowBytes + x * 4;
                    const offset = (((tileY - startY) * kCellSize + y) * width
                        + (tileX - startX) * kCellSize + x) * 4;
                    image.data[offset] = base.sheet.pixels[source];
                    image.data[offset + 1] = base.sheet.pixels[source + 1];
                    image.data[offset + 2] = base.sheet.pixels[source + 2];
                    image.data[offset + 3] = 255;
                    for(const layer of layers)
                    {
                        const alpha = cellCoverage(project, layer.masks, x, y);
                        for(let channel = 0; channel < 3; channel++)
                        {
                            image.data[offset + channel] = Math.round((image.data[offset + channel] * (255 - alpha)
                                + layer.pixels[source + channel] * alpha) / 255);
                        }
                    }
                }
            }
        }
    }
    const originX = startX * kCellSize;
    const originY = startY * kCellSize;
    context.putImageData(image, originX, originY);
}

/// Draw only the selected layer's alpha, retaining full mask resolution.
export function drawCoverageRegion(project: ProjectDocument, canvas: HTMLCanvasElement, startX: number, startY: number, endX: number, endY: number): void
{
    const context = prepareCanvas(canvas);
    const width = (endX - startX) * kCellSize;
    const height = (endY - startY) * kCellSize;
    const coverage = context.createImageData(width, height);
    coverage.data.fill(255);
    const selected = project.layers.findIndex((layer) => layer.id === project.activeLayerId);
    if(selected !== 0)
    {
        for(let tileY = startY; tileY < endY; tileY++)
        {
            for(let tileX = startX; tileX < endX; tileX++)
            {
                const cell = tileY * kGridSize + tileX;
                const layer = project.layers[selected];
                const masks = cellMasks(project, layer, cell);
                for(let y = 0; y < kCellSize; y++)
                {
                    for(let x = 0; x < kCellSize; x++)
                    {
                        const offset = (((tileY - startY) * kCellSize + y) * width
                            + (tileX - startX) * kCellSize + x) * 4;
                        coverage.data[offset + 3] = cellCoverage(project, masks, x, y);
                    }
                }
            }
        }
    }
    context.putImageData(coverage, startX * kCellSize, startY * kCellSize);
}


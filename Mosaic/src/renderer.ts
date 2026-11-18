import { type SelectionBox } from "./props.ts";
import { type ProjectDocument, type MaskPlacement, type PropAsset, type ImageAsset } from "./document.ts";
import { cellMasks, sampleMask, terrainSourceOffset } from "./placement.ts";
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
            // Equal texture dimensions give every layer the same source offset.
            const start = terrainSourceOffset(project, base, cell, 0, 0);
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


export interface ImagePreview
{
    readonly asset: PropAsset | ImageAsset;
    /// Optional draw dimensions in map pixels. Omit to retain native image size.
    readonly size?: { readonly width: number; readonly height: number };
    readonly x: number;
    readonly y: number;
}

// Weak keys keep decoded preview canvases alive only as long as their source assets.
const terrainPreviewImages = new WeakMap<ImageAsset, OffscreenCanvas>();

function terrainPreviewImage(asset: ImageAsset): OffscreenCanvas
{
    const cached = terrainPreviewImages.get(asset);
    if(cached) { return cached; }
    const canvas = new OffscreenCanvas(asset.dimension, asset.dimension);
    const context = canvas.getContext("2d");
    if(context === null) { throw new Error("Canvas 2D is unavailable"); }
    const image = context.createImageData(asset.dimension, asset.dimension);
    image.data.set(asset.pixels);
    // Terrain ignores source alpha, matching the painted map rather than the image file.
    for(let offset = 3; offset < image.data.length; offset += 4) { image.data[offset] = 255; }
    context.putImageData(image, 0, 0);
    terrainPreviewImages.set(asset, canvas);
    return canvas;
}

/// A transparent overlay keeps previews and selection separate from terrain rendering.
export function drawProps(project: ProjectDocument, canvas: HTMLCanvasElement, selectedIds: ReadonlySet<number>,
    preview: ImagePreview | null, box: SelectionBox | null): void
{
    const context = prepareCanvas(canvas);
    context.clearRect(0, 0, canvas.width, canvas.height);
    context.imageSmoothingEnabled = false;
    for(const prop of project.props)
    {
        context.save();
        context.translate(prop.x, prop.y);
        context.rotate(prop.rotation * Math.PI / 180);
        context.drawImage(prop.asset.image, -prop.asset.width / 2, -prop.asset.height / 2);
        context.restore();
    }
    for(const selected of project.props)
    {
        if(!selectedIds.has(selected.id)) { continue; }
        context.save();
        context.translate(selected.x, selected.y);
        context.rotate(selected.rotation * Math.PI / 180);
        context.strokeStyle = "#ffdc60";
        context.lineWidth = 2 / project.view.zoom;
        context.strokeRect(-selected.asset.width / 2, -selected.asset.height / 2,
            selected.asset.width, selected.asset.height);
        context.restore();
    }
    if(preview)
    {
        context.save();
        context.globalAlpha = 0.5;
        const asset = preview.asset;
        const image = asset.kind === "prop" ? asset.image : terrainPreviewImage(asset);
        const width = preview.size?.width ?? (asset.kind === "prop" ? asset.width : asset.dimension);
        const height = preview.size?.height ?? (asset.kind === "prop" ? asset.height : asset.dimension);
        if(preview.size)
        {
            context.drawImage(image, preview.x - width / 2, preview.y - height / 2, width, height);
        }
        else { context.drawImage(image, preview.x - width / 2, preview.y - height / 2); }
        context.restore();
    }
    if(box)
    {
        context.save();
        context.strokeStyle = "#ffdc60";
        context.fillStyle = "rgba(255, 220, 96, 0.12)";
        context.lineWidth = 1 / project.view.zoom;
        const x = Math.min(box.start.x, box.end.x);
        const y = Math.min(box.start.y, box.end.y);
        const width = Math.abs(box.end.x - box.start.x);
        const height = Math.abs(box.end.y - box.start.y);
        context.fillRect(x, y, width, height);
        context.strokeRect(x, y, width, height);
        context.restore();
    }
}

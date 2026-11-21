import { mapPixels, propImageRect, type SelectionBox } from "./props.ts";
import { type ProjectDocument, type MaskPlacement, type PropAsset, type ImageAsset } from "./document.ts";
import { cellMasks, sampleMask, terrainSourceOffset } from "./placement.ts";
import { kCellSize, kHalfCellSize } from "./grid.ts";

/// A viewport origin is in map pixels. Width and height are display pixels.
export interface RenderView
{
    readonly x: number;
    readonly y: number;
    readonly width: number;
    readonly height: number;
    readonly zoom: number;
}

function prepareCanvas(canvas: HTMLCanvasElement, view: RenderView): CanvasRenderingContext2D
{
    if(canvas.width !== view.width || canvas.height !== view.height)
    {
        canvas.width = view.width;
        canvas.height = view.height;
    }
    const context = canvas.getContext("2d");
    if(context === null) { throw new Error("Canvas 2D is unavailable"); }
    return context;
}

function cellCoverage(project: ProjectDocument, masks: readonly (MaskPlacement | null)[], x: number, y: number): number
{
    const quadrant = (y >= kHalfCellSize ? 2 : 0) + (x >= kHalfCellSize ? 1 : 0);
    return sampleMask(masks[quadrant], project.masks,
        (x + kHalfCellSize) % kCellSize, (y + kHalfCellSize) % kCellSize);
}

/// Sample display-pixel centers directly, including when zoomed out. Temporary
/// pixels and mask choices belong only to this draw, with no terrain cache.
function drawRegion(project: ProjectDocument, canvas: HTMLCanvasElement, view: RenderView,
    startX: number, startY: number, endX: number, endY: number, coverageOnly: boolean): void
{
    const resized = canvas.width !== view.width || canvas.height !== view.height;
    const context = prepareCanvas(canvas, view);
    if(resized) { startX = 0; startY = 0; endX = project.gridSize; endY = project.gridSize; }
    const left = Math.max(0, Math.ceil((startX * kCellSize - view.x) * view.zoom - 0.5));
    const top = Math.max(0, Math.ceil((startY * kCellSize - view.y) * view.zoom - 0.5));
    const right = Math.min(view.width, Math.ceil((endX * kCellSize - view.x) * view.zoom - 0.5));
    const bottom = Math.min(view.height, Math.ceil((endY * kCellSize - view.y) * view.zoom - 0.5));
    if(right <= left || bottom <= top) { return; }
    const width = right - left;
    const image = context.createImageData(width, bottom - top);
    const base = project.layers[0];
    const selected = project.layers.findIndex((layer) => layer.id === project.activeLayerId);
    const layers = coverageOnly ? (selected === 0 ? [] : [project.layers[selected]]) : project.layers.slice(1);
    for(let y = top; y < bottom; y++)
    {
        const worldY = Math.floor(view.y + (y + 0.5) / view.zoom);
        const tileY = Math.floor(worldY / kCellSize);
        let lastCell = -1;
        let masks: (readonly (MaskPlacement | null)[])[] = [];
        for(let x = left; x < right; x++)
        {
            const worldX = Math.floor(view.x + (x + 0.5) / view.zoom);
            const tileX = Math.floor(worldX / kCellSize);
            const cell = tileY * project.gridSize + tileX;
            if(cell !== lastCell)
            {
                masks = layers.map((layer) => cellMasks(project, layer, cell));
                lastCell = cell;
            }
            const localX = worldX % kCellSize, localY = worldY % kCellSize;
            const offset = ((y - top) * width + x - left) * 4;
            if(coverageOnly)
            {
                image.data[offset] = image.data[offset + 1] = image.data[offset + 2] = 255;
                image.data[offset + 3] = selected === 0 ? 255 : cellCoverage(project, masks[0], localX, localY);
                continue;
            }
            const source = terrainSourceOffset(project, base, cell, localX, localY);
            image.data[offset] = base.sheet.pixels[source];
            image.data[offset + 1] = base.sheet.pixels[source + 1];
            image.data[offset + 2] = base.sheet.pixels[source + 2];
            image.data[offset + 3] = 255;
            for(let index = 0; index < layers.length; index++)
            {
                const alpha = cellCoverage(project, masks[index], localX, localY);
                for(let channel = 0; channel < 3; channel++)
                {
                    image.data[offset + channel] = Math.round((image.data[offset + channel] * (255 - alpha)
                        + layers[index].sheet.pixels[source + channel] * alpha) / 255);
                }
            }
        }
    }
    context.putImageData(image, left, top);
}

/// Dirty bounds are whole map cells, including affected junction neighbors.
export function drawTerrainRegion(project: ProjectDocument, canvas: HTMLCanvasElement, view: RenderView,
    startX: number, startY: number, endX: number, endY: number): void
{
    drawRegion(project, canvas, view, startX, startY, endX, endY, false);
}

export function drawCoverageRegion(project: ProjectDocument, canvas: HTMLCanvasElement, view: RenderView,
    startX: number, startY: number, endX: number, endY: number): void
{
    drawRegion(project, canvas, view, startX, startY, endX, endY, true);
}


export interface ImagePreview
{
    readonly asset: PropAsset | ImageAsset;
    /// Optional terrain preview dimensions in map pixels. Props use their asset scale.
    readonly size?: { readonly width: number; readonly height: number };
    /// Prop pivot or terrain preview center in fractional cells.
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
export function drawProps(project: ProjectDocument, canvas: HTMLCanvasElement, view: RenderView, selectedIds: ReadonlySet<number>,
    preview: ImagePreview | null, box: SelectionBox | null, showBounds: boolean): void
{
    const context = prepareCanvas(canvas, view);
    context.clearRect(0, 0, canvas.width, canvas.height);
    context.imageSmoothingEnabled = false;
    context.save();
    context.setTransform(view.zoom, 0, 0, view.zoom, -view.x * view.zoom, -view.y * view.zoom);
    const visible = (prop: ProjectDocument["props"][number]): boolean => {
        const point = mapPixels(prop, project.gridSize);
        const angle = prop.rotation * Math.PI / 180;
        const rect = propImageRect(prop.asset);
        const cos = Math.cos(angle), sin = Math.sin(angle);
        const offsetX = rect.x + rect.width / 2, offsetY = rect.y + rect.height / 2;
        const centerX = point.x + offsetX * cos - offsetY * sin;
        const centerY = point.y + offsetX * sin + offsetY * cos;
        const halfWidth = (Math.abs(cos) * rect.width + Math.abs(sin) * rect.height) / 2;
        const halfHeight = (Math.abs(sin) * rect.width + Math.abs(cos) * rect.height) / 2;
        const margin = 2 / view.zoom;
        return centerX + halfWidth + margin >= view.x && centerY + halfHeight + margin >= view.y
            && centerX - halfWidth - margin <= view.x + view.width / view.zoom
            && centerY - halfHeight - margin <= view.y + view.height / view.zoom;
    };
    for(const prop of project.props)
    {
        if(!visible(prop)) { continue; }
        context.save();
        const point = mapPixels(prop, project.gridSize);
        context.translate(point.x, point.y);
        context.rotate(prop.rotation * Math.PI / 180);
        const rect = propImageRect(prop.asset);
        const source = prop.asset.srcRect;
        context.drawImage(prop.asset.image, source.x, source.y, source.w, source.h,
            rect.x, rect.y, rect.width, rect.height);
        context.restore();
    }
    if(showBounds)
    {
        // Bounds can extend beyond the image, so image culling must not hide them.
        for(const prop of project.props)
        {
            if(prop.asset.bounds.length === 0) { continue; }
            context.save();
            const point = mapPixels(prop, project.gridSize);
            context.translate(point.x, point.y);
            context.rotate(prop.rotation * Math.PI / 180);
            context.strokeStyle = "#00e5ff";
            context.lineWidth = 2 / view.zoom;
            const { width, height } = propImageRect(prop.asset);
            for(const bound of prop.asset.bounds)
            {
                context.strokeRect((bound.x - prop.asset.pivotX) * width,
                    (bound.y - prop.asset.pivotY) * height, bound.w * width, bound.h * height);
            }
            context.restore();
        }
    }
    for(const selected of project.props)
    {
        if(!selectedIds.has(selected.id) || !visible(selected)) { continue; }
        context.save();
        const point = mapPixels(selected, project.gridSize);
        context.translate(point.x, point.y);
        context.rotate(selected.rotation * Math.PI / 180);
        context.strokeStyle = "#ffdc60";
        context.lineWidth = 2 / view.zoom;
        const rect = propImageRect(selected.asset);
        context.strokeRect(rect.x, rect.y, rect.width, rect.height);
        context.restore();
    }
    if(preview)
    {
        context.save();
        context.globalAlpha = 0.5;
        const point = mapPixels(preview, project.gridSize);
        const asset = preview.asset;
        if(asset.kind === "prop")
        {
            const rect = propImageRect(asset);
            const source = asset.srcRect;
            context.drawImage(asset.image, source.x, source.y, source.w, source.h,
                point.x + rect.x, point.y + rect.y, rect.width, rect.height);
        }
        else
        {
            const width = preview.size?.width ?? asset.dimension;
            const height = preview.size?.height ?? asset.dimension;
            context.drawImage(terrainPreviewImage(asset), point.x - width / 2, point.y - height / 2, width, height);
        }
        context.restore();
    }
    if(box)
    {
        context.save();
        context.strokeStyle = "#ffdc60";
        context.fillStyle = "rgba(255, 220, 96, 0.12)";
        context.lineWidth = 1 / view.zoom;
        const start = mapPixels(box.start, project.gridSize);
        const end = mapPixels(box.end, project.gridSize);
        const x = Math.min(start.x, end.x);
        const y = Math.min(start.y, end.y);
        const width = Math.abs(box.end.x - box.start.x) * kCellSize;
        const height = Math.abs(box.end.y - box.start.y) * kCellSize;
        context.fillRect(x, y, width, height);
        context.strokeRect(x, y, width, height);
        context.restore();
    }
    // Draw pivots last in screen pixels so they stay upright and readable at every zoom.
    for(const selected of project.props)
    {
        if(!selectedIds.has(selected.id)) { continue; }
        const point = mapPixels(selected, project.gridSize);
        const x = (point.x - view.x) * view.zoom;
        const y = (point.y - view.y) * view.zoom;
        if(x + 7 < 0 || y + 7 < 0 || x - 7 > canvas.width || y - 7 > canvas.height) { continue; }
        context.save();
        context.setTransform(1, 0, 0, 1, 0, 0);
        context.fillStyle = "#161b16";
        context.fillRect(x - 7, y - 2, 14, 4);
        context.fillRect(x - 2, y - 7, 4, 14);
        context.fillStyle = "#ff00ff";
        context.fillRect(x - 6, y - 1, 12, 2);
        context.fillRect(x - 1, y - 6, 2, 12);
        context.restore();
    }
    context.restore();
}

import { kCellSize } from "./grid.ts";
import { type Placement, type SpriteSequence } from "./document.ts";

/// Scaled sprite bounds in Y-down pixels relative to the pivot, before rotation.
export function propImageRect(asset: SpriteSequence): { x: number; y: number; width: number; height: number }
{
    return imageRect(asset, asset.scale);
}

export function placementScale(placement: Placement): number
{
    return placement.scale ?? placement.definition.sprite.sequences[0].scale;
}

export function placementImageRect(placement: Placement): { x: number; y: number; width: number; height: number }
{
    return imageRect(placement.definition.sprite.sequences[0], placementScale(placement));
}

function imageRect(asset: SpriteSequence, scale: number): { x: number; y: number; width: number; height: number }
{
    const width = (asset.size?.w ?? asset.srcRect.w) * scale;
    const height = (asset.size?.h ?? asset.srcRect.h) * scale;
    return { x: -asset.pivotX * width, y: -asset.pivotY * height, width, height };
}

/// Convert client pixels to centered, Y-up fractional cells using the full map surface bounds.
export function mapPoint(clientX: number, clientY: number,
    bounds: { left: number; top: number; width: number; height: number }, size: number): { x: number; y: number }
{
    return { x: (clientX - bounds.left) * size / bounds.width - size / 2,
        y: size / 2 - (clientY - bounds.top) * size / bounds.height };
}

/// Convert centered, Y-up cells to top-left, Y-down map pixels for drawing.
export function mapPixels(point: Point, size: number): Point
{
    return { x: (point.x + size / 2) * kCellSize, y: (size / 2 - point.y) * kCellSize };
}

export function clampPropPoint(x: number, y: number, size: number): { x: number; y: number }
{
    return { x: Math.max(-size / 2, Math.min(size / 2, x)), y: Math.max(-size / 2, Math.min(size / 2, y)) };
}

/// Pick the last drawn rectangle under the pointer, including transparent pixels.
export function hitProp(props: readonly Placement[], x: number, y: number): Placement | null
{
    for(let index = props.length - 1; index >= 0; index--)
    {
        const prop = props[index];
        const angle = prop.rotation * Math.PI / 180;
        const dx = (x - prop.x) * kCellSize;
        const dy = (prop.y - y) * kCellSize;
        const localX = dx * Math.cos(angle) + dy * Math.sin(angle);
        const localY = -dx * Math.sin(angle) + dy * Math.cos(angle);
        const rect = placementImageRect(prop);
        if(localX >= rect.x && localX <= rect.x + rect.width
            && localY >= rect.y && localY <= rect.y + rect.height)
        {
            return prop;
        }
    }
    return null;
}

export interface Point { readonly x: number; readonly y: number; }
/// Selection bounds use centered, Y-up fractional cells.
export interface SelectionBox { readonly start: Point; readonly end: Point; }

/// Rectangle selection uses pivots, independently of sprite rotation and transparent margins.
export function propsInBox(props: readonly Placement[], box: SelectionBox): number[]
{
    const left = Math.min(box.start.x, box.end.x);
    const right = Math.max(box.start.x, box.end.x);
    const top = Math.min(box.start.y, box.end.y);
    const bottom = Math.max(box.start.y, box.end.y);
    return props.filter((prop) => prop.x >= left && prop.x <= right && prop.y >= top && prop.y <= bottom)
        .map((prop) => prop.id);
}

/// Clamp one shared translation so every pivot stays in bounds without deforming the group.
export function groupMoveDelta(points: readonly Point[], dx: number, dy: number, size: number): Point
{
    if(points.length === 0) { return { x: 0, y: 0 }; }
    const half = size / 2;
    let minX = half, minY = half, maxX = -half, maxY = -half;
    for(const point of points)
    {
        minX = Math.min(minX, point.x);
        minY = Math.min(minY, point.y);
        maxX = Math.max(maxX, point.x);
        maxY = Math.max(maxY, point.y);
    }
    return { x: Math.max(-half - minX, Math.min(half - maxX, dx)),
        y: Math.max(-half - minY, Math.min(half - maxY, dy)) };
}

export interface ScaleHandle extends Point
{
    readonly cursor: "ew-resize" | "nwse-resize" | "ns-resize" | "nesw-resize";
}

/// Handle centers in centered, Y-up cells. A corner at the pivot cannot scale.
export function scaleHandles(placement: Placement): ScaleHandle[]
{
    const rect = placementImageRect(placement);
    const angle = placement.rotation * Math.PI / 180;
    const cos = Math.cos(angle), sin = Math.sin(angle);
    return [{ x: rect.x, y: rect.y }, { x: rect.x + rect.width, y: rect.y },
        { x: rect.x + rect.width, y: rect.y + rect.height }, { x: rect.x, y: rect.y + rect.height }]
        .map((point, corner) => ({ ...point,
            // Quantize the rotated corner bisector to the nearest native resize cursor.
            cursor: (["ew-resize", "nwse-resize", "ns-resize", "nesw-resize"] as const)[
                Math.round((placement.rotation + (corner % 2 === 0 ? 45 : 135)) / 45) % 4] }))
        .filter((point) => point.x !== 0 || point.y !== 0)
        .map((point) => ({ x: placement.x + (point.x * cos - point.y * sin) / kCellSize,
            y: placement.y - (point.x * sin + point.y * cos) / kCellSize, cursor: point.cursor }));
}

/// Hit targets remain fourteen screen pixels wide, independent of zoom.
export function hitScaleHandle(placement: Placement, point: Point, zoom: number): ScaleHandle | null
{
    const radius = 7 / (kCellSize * zoom);
    return scaleHandles(placement).filter((handle) => Math.abs(handle.x - point.x) <= radius
        && Math.abs(handle.y - point.y) <= radius)
        .sort((a, b) => Math.hypot(a.x - point.x, a.y - point.y) - Math.hypot(b.x - point.x, b.y - point.y))[0] ?? null;
}

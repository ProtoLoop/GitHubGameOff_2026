import { type Prop } from "./document.ts";
import { kCellSize, kGridSize } from "./grid.ts";

/// Coordinates use map pixels, with positive Y downward and clockwise degrees.
export function mapPoint(clientX: number, clientY: number,
    bounds: { left: number; top: number; width: number; height: number }): { x: number; y: number }
{
    const size = kGridSize * kCellSize;
    return { x: (clientX - bounds.left) * size / bounds.width,
        y: (clientY - bounds.top) * size / bounds.height };
}

export function clampPropPoint(x: number, y: number): { x: number; y: number }
{
    const size = kGridSize * kCellSize;
    return { x: Math.max(0, Math.min(size, x)), y: Math.max(0, Math.min(size, y)) };
}

/// Pick the last drawn rectangle under the pointer, including transparent pixels.
export function hitProp(props: readonly Prop[], x: number, y: number): Prop | null
{
    for(let index = props.length - 1; index >= 0; index--)
    {
        const prop = props[index];
        const angle = prop.rotation * Math.PI / 180;
        const dx = x - prop.x;
        const dy = y - prop.y;
        const localX = dx * Math.cos(angle) + dy * Math.sin(angle);
        const localY = -dx * Math.sin(angle) + dy * Math.cos(angle);
        if(Math.abs(localX) <= prop.asset.width / 2 && Math.abs(localY) <= prop.asset.height / 2)
        {
            return prop;
        }
    }
    return null;
}

export interface Point { readonly x: number; readonly y: number; }
export interface SelectionBox { readonly start: Point; readonly end: Point; }

/// Rectangle selection uses centers, independently of sprite rotation and transparent margins.
export function propsInBox(props: readonly Prop[], box: SelectionBox): number[]
{
    const left = Math.min(box.start.x, box.end.x);
    const right = Math.max(box.start.x, box.end.x);
    const top = Math.min(box.start.y, box.end.y);
    const bottom = Math.max(box.start.y, box.end.y);
    return props.filter((prop) => prop.x >= left && prop.x <= right && prop.y >= top && prop.y <= bottom)
        .map((prop) => prop.id);
}

/// Clamp one shared translation so every center stays in bounds without deforming the group.
export function groupMoveDelta(points: readonly Point[], dx: number, dy: number): Point
{
    if(points.length === 0) { return { x: 0, y: 0 }; }
    const size = kGridSize * kCellSize;
    let minX = size, minY = size, maxX = 0, maxY = 0;
    for(const point of points)
    {
        minX = Math.min(minX, point.x);
        minY = Math.min(minY, point.y);
        maxX = Math.max(maxX, point.x);
        maxY = Math.max(maxY, point.y);
    }
    return { x: Math.max(-minX, Math.min(size - maxX, dx)),
        y: Math.max(-minY, Math.min(size - maxY, dy)) };
}

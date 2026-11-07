import { type ImageAsset, type Layer, type MaskPlacement, type ProjectDocument, type Rotation, type TilePlacement } from "./document.ts";

// Eight unique square symmetries, expressed as source-axis flips followed by
// clockwise rotation. The order preserves the existing placement choices.
const orientations: readonly { rotation: Rotation; hflip: boolean; vflip: boolean }[] = [
    { rotation: "0", hflip: false, vflip: false },
    { rotation: "90", hflip: false, vflip: false },
    { rotation: "180", hflip: false, vflip: false },
    { rotation: "270", hflip: false, vflip: false },
    { rotation: "0", hflip: true, vflip: false },
    { rotation: "0", hflip: false, vflip: true },
    { rotation: "90", hflip: false, vflip: true },
    { rotation: "90", hflip: true, vflip: false }
];
const patternOrientations = Array.from({ length: 16 }, (_, pattern) => orientations.filter((orientation) => {
    for(let bit = 0; bit < 4; bit++)
    {
        const source = maskSource(orientation, bit % 2, Math.floor(bit / 2), 1);
        if(((pattern >>> (source.y * 2 + source.x)) & 1) !== ((pattern >>> bit) & 1)) { return false; }
    }
    return true;
}));

function hash(x: number, y: number, seed: number): number
{
    let value = Math.imul(x ^ seed, 0x45d9f3b);
    value = Math.imul(value ^ y, 0x45d9f3b);
    value ^= value >>> 16;
    return value >>> 0;
}

/// Deterministic terrain orientation, independent of painted occupancy.
export function createTile(layerIndex: number, cell: number, tile: number): TilePlacement
{
    const flip = hash(cell % 64, Math.floor(cell / 64), hash(layerIndex, 0, 3571)) % 4;
    return { cell, tile, hflip: (flip & 1) !== 0, vflip: (flip & 2) !== 0 };
}

export function automaticTile(project: ProjectDocument, layer: Layer, cell: number): TilePlacement
{
    const layerIndex = project.layers.indexOf(layer);
    const tile = hash(cell % 64, Math.floor(cell / 64), layerIndex) % (layer.atlasGrid * layer.atlasGrid);
    return createTile(layerIndex, cell, tile);
}

function resolveMask(project: ProjectDocument, layer: Layer, cell: number): MaskPlacement | null
{
    const jx = cell % 65;
    const jy = Math.floor(cell / 65);
    let pattern = 0;
    for(let bit = 0; bit < 4; bit++)
    {
        const x = jx - 1 + bit % 2;
        const y = jy - 1 + Math.floor(bit / 2);
        if(x >= 0 && x < 64 && y >= 0 && y < 64 && layer.painted[y * 64 + x])
        {
            pattern |= 1 << bit;
        }
    }
    if(pattern === 0) { return null; }
    const layerIndex = project.layers.indexOf(layer);
    const choices = patternOrientations[pattern];
    const orientation = choices[hash(jx, jy, hash(layerIndex, pattern, 7919)) % choices.length];
    const sheet = project.maskMode === "random"
        ? hash(jx, jy, 1201 + layerIndex) % project.masks.length : project.maskMode;
    return { sheet, cell, tile: pattern, ...orientation };
}

/// Output-to-source lookup reverses the clockwise rotation before undoing the
/// source-axis flips. `last` is the last source pixel coordinate.
export function maskSource(mask: { rotation: Rotation; hflip: boolean; vflip: boolean },
    x: number, y: number, last: number): { x: number; y: number }
{
    let sourceX = x;
    let sourceY = y;
    switch(mask.rotation)
    {
    case "0": break;
    case "90": sourceX = y; sourceY = last - x; break;
    case "180": sourceX = last - x; sourceY = last - y; break;
    case "270": sourceX = last - y; sourceY = x; break;
    }
    return { x: mask.hflip ? last - sourceX : sourceX, y: mask.vflip ? last - sourceY : sourceY };
}

/// Sample a resolved mask using the shared coverage convention.
export function sampleMask(mask: MaskPlacement | null, sheets: readonly ImageAsset[], x: number, y: number): number
{
    if(mask === null) { return 0; }
    const source = maskSource(mask, x, y, 63);
    return sheets[mask.sheet].coverage[mask.tile * 4096 + source.y * 64 + source.x];
}

export function cellMasks(project: ProjectDocument, layer: Layer, cell: number): readonly (MaskPlacement | null)[]
{
    const junction = Math.floor(cell / 64) * 65 + cell % 64;
    return [0, 1, 65, 66].map((offset) => resolveMask(project, layer, junction + offset));
}

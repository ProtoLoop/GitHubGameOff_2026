export interface ImageAsset
{
    readonly id: string;
    readonly kind: "tile" | "mask";
    readonly label: string;
    readonly blob: Blob;
    readonly dimension: number;
    readonly pixels: Uint8ClampedArray;
    readonly coverage: Uint8Array;
    readonly url: string;
}

export type Rotation = "0" | "90" | "180" | "270";

export interface TilePlacement
{
    readonly cell: number;
    readonly tile: number;
    readonly hflip: boolean;
    readonly vflip: boolean;
}

export interface MaskPlacement extends TilePlacement
{
    readonly sheet: number;
    readonly rotation: Rotation;
}

export interface Layer
{
    readonly id: number;
    /// Dense occupancy in memory, sparse painted-cell records in the file.
    readonly painted: boolean[];
    sheet: ImageAsset;
    atlasGrid: number;
    tileSize: number;
}

export interface ProjectDocument
{
    readonly gridSize: 64;
    layers: Layer[];
    tileSheets: readonly ImageAsset[];
    masks: readonly ImageAsset[];
    tileSize: number;
    nextLayerId: number;
    activeLayerId: number;
    maskMode: "random" | number;
    view: { zoom: number; scrollLeft: number; scrollTop: number; showGrid: boolean };
}

export function atlasGrid(dimension: number, tileSize: number): number
{
    const count = dimension / tileSize;
    return Number.isInteger(count) && count >= 2 && count % 2 === 0 ? count : 0;
}

export function createDocument(tiles: readonly ImageAsset[], masks: readonly ImageAsset[]): ProjectDocument
{
    if(tiles.length === 0 || masks.length === 0)
    {
        throw new Error("A new project needs at least one tile sheet and one mask set.");
    }
    const sheets = [tiles[0], tiles[Math.min(1, tiles.length - 1)]];
    const project: ProjectDocument = {
        gridSize: 64, tileSize: 64, nextLayerId: 2, activeLayerId: 1,
        tileSheets: [...tiles], masks: [...masks], maskMode: "random",
        view: { zoom: 1, scrollLeft: 0, scrollTop: 0, showGrid: true },
        layers: sheets.map((sheet, id) => ({
            id, sheet, painted: Array(4096).fill(id === 0),
            atlasGrid: atlasGrid(sheet.dimension, 64), tileSize: 64
        }))
    };
    validateDocument(project);
    return project;
}

/// Snapshots own their map arrays but share immutable image assets.
export function cloneDocument(project: ProjectDocument): ProjectDocument
{
    return {
        ...project, view: { ...project.view }, tileSheets: [...project.tileSheets], masks: [...project.masks],
        layers: project.layers.map((layer) => ({ ...layer, painted: [...layer.painted] }))
    };
}

export function documentBytes(project: ProjectDocument): number
{
    // Conservative estimate for boolean array slots, excluding shared images.
    return project.layers.reduce((total, layer) => total + 128 + layer.painted.length * 8,
        256 + (project.tileSheets.length + project.masks.length) * 8);
}

/// View changes and swatch selection do not create undo actions.
export function sameContent(a: ProjectDocument, b: ProjectDocument): boolean
{
    if(a.tileSize !== b.tileSize || a.nextLayerId !== b.nextLayerId || a.maskMode !== b.maskMode
        || a.layers.length !== b.layers.length || a.masks.length !== b.masks.length
        || a.tileSheets.length !== b.tileSheets.length
        || a.tileSheets.some((sheet, index) => sheet !== b.tileSheets[index])
        || a.masks.some((mask, index) => mask !== b.masks[index]))
    {
        return false;
    }
    return a.layers.every((layer, index) => {
        const other = b.layers[index];
        return layer.id === other.id && layer.sheet === other.sheet
            && layer.painted.every((painted, cell) => painted === other.painted[cell]);
    });
}

export function documentAssets(project: ProjectDocument): ImageAsset[]
{
    return [...new Set([...project.tileSheets, ...project.layers.map((layer) => layer.sheet), ...project.masks])];
}

export function validateDocument(project: ProjectDocument): void
{
    const fail = (message: string): never => { throw new Error(`Invalid project: ${message}`); };
    if(project.gridSize !== 64 || !Number.isSafeInteger(project.tileSize) || project.tileSize < 1
        || project.tileSheets.length < 1 || project.tileSheets.length > 256
        || project.layers.length < 1 || project.layers.length > 256 || project.masks.length < 1
        || project.masks.length > 64)
    {
        fail("unsupported map dimensions, tile size, or layer/mask count");
    }
    const ids = new Set<number>();
    for(const [index, layer] of project.layers.entries())
    {
        if(!Number.isSafeInteger(layer.id) || layer.id < 0 || ids.has(layer.id))
        {
            fail("layer IDs must be unique nonnegative integers");
        }
        ids.add(layer.id);
        const grid = atlasGrid(layer.sheet.dimension, project.tileSize);
        if(layer.sheet.kind !== "tile" || !project.tileSheets.includes(layer.sheet) || grid === 0 || layer.atlasGrid !== grid
            || layer.tileSize !== project.tileSize)
        {
            fail(`${layer.sheet.label} is incompatible with the tile size`);
        }
        if(layer.painted.length !== 4096 || layer.painted.some((value) => typeof value !== "boolean"))
        {
            fail("invalid painted-cell grid");
        }
        if(index === 0 && layer.painted.some((value) => !value))
        {
            fail("the base must fill every cell");
        }
    }
    if(!Number.isSafeInteger(project.nextLayerId) || project.nextLayerId <= Math.max(...ids)
        || !ids.has(project.activeLayerId))
    {
        fail("invalid next or selected layer ID");
    }
    if(project.tileSheets.some((sheet) => sheet.kind !== "tile") || project.masks.some((mask) => mask.kind !== "mask" || mask.dimension !== 256)
        || (project.maskMode !== "random" && (!Number.isInteger(project.maskMode)
            || project.maskMode < 0 || project.maskMode >= project.masks.length)))
    {
        fail("invalid mask set or selection");
    }
    const assets = documentAssets(project);
    if(new Set(project.tileSheets).size !== project.tileSheets.length
        || new Set(project.masks).size !== project.masks.length
        || new Set(assets.map((asset) => asset.id)).size !== assets.length
        || assets.some((asset) => !/^assets\/(tiles|masks)\/[^/\\]+$/.test(asset.id)
            || !asset.label || asset.label.length > 1024))
    {
        fail("invalid or conflicting asset identity or filename");
    }
    const view = project.view;
    if(!Number.isFinite(view.zoom) || view.zoom < 1 / 16 || view.zoom > 4
        || !Number.isFinite(view.scrollLeft) || view.scrollLeft < 0 || view.scrollLeft > 16384
        || !Number.isFinite(view.scrollTop) || view.scrollTop < 0 || view.scrollTop > 16384
        || typeof view.showGrid !== "boolean")
    {
        fail("invalid view settings");
    }
}

/// Decode once. The returned bytes and pixels are immutable for the asset lifetime.
export async function decodeAsset(blob: Blob, label: string, id: string, kind: "tile" | "mask"): Promise<ImageAsset>
{
    let bitmap: ImageBitmap;
    try { bitmap = await createImageBitmap(blob); }
    catch { throw new Error(`Could not decode ${label}`); }
    try
    {
        const dimension = bitmap.width;
        if(dimension !== bitmap.height || dimension < 1 || dimension > 4096
            || (kind === "mask" && dimension !== 256))
        {
            throw new Error(`${label}: ${kind === "mask" ? "masks must be 256×256" : "tile sheets must be square and at most 4096×4096"}`);
        }
        const canvas = new OffscreenCanvas(dimension, dimension);
        const context = canvas.getContext("2d");
        if(context === null) { throw new Error("Canvas 2D is unavailable"); }
        context.drawImage(bitmap, 0, 0);
        const pixels = context.getImageData(0, 0, dimension, dimension).data;
        const coverage = new Uint8Array(kind === "mask" ? 65536 : 0);
        if(kind === "mask")
        {
            for(let pattern = 0; pattern < 16; pattern++)
            {
                for(let y = 0; y < 64; y++)
                {
                    for(let x = 0; x < 64; x++)
                    {
                        coverage[pattern * 4096 + y * 64 + x] = pixels[
                            ((Math.floor(pattern / 4) * 64 + y) * 256 + pattern % 4 * 64 + x) * 4];
                    }
                }
            }
        }
        return { id, kind, label, blob, dimension, pixels, coverage, url: URL.createObjectURL(blob) };
    }
    finally { bitmap.close(); }
}

import { kDefaultGridSize, validGridSize, kCellSize, kMaskAtlasGridSize, kMaskAtlasPixelSize, kMaskPixelCount } from "./grid.ts";

export interface Asset
{
    readonly id: string;
    readonly label: string;
    readonly blob: Blob;
    readonly url: string;
}

/// Normalized cropped-sprite rectangle: (0, 0) is upper-left, (1, 1) is lower-right before transforms.
export interface PropCollisionRect
{
    readonly x: number;
    readonly y: number;
    readonly w: number;
    readonly h: number;
}

export interface PropAsset extends Asset
{
    readonly kind: "prop";
    readonly width: number;
    readonly height: number;
    readonly image: CanvasImageSource;
    /// Source texture pixels, measured from the upper-left corner.
    readonly srcRect: { readonly x: number; readonly y: number; readonly w: number; readonly h: number };
    /// Normalized sprite pivot, measured from the cropped sprite's upper-left corner.
    readonly pivotX: number;
    readonly pivotY: number;
    readonly scale: number;
    readonly collisionRects: readonly PropCollisionRect[];
}

export interface Prop
{
    readonly id: number;
    readonly asset: PropAsset;
    /// Pivot in centered, Y-up fractional cells, rounded to three decimal places during editing.
    x: number;
    y: number;
    rotation: number;
}

export interface ImageAsset extends Asset
{
    readonly kind: "tile" | "mask";
    readonly dimension: number;
    readonly pixels: Uint8ClampedArray;
    readonly coverage: Uint8Array;
}

export type Rotation = "0" | "90" | "180" | "270";

export interface MaskPlacement
{
    readonly cell: number;
    readonly tile: number;
    readonly hflip: boolean;
    readonly vflip: boolean;
    readonly sheet: number;
    readonly rotation: Rotation;
}

export interface Layer
{
    readonly id: number;
    /// Dense occupancy in memory, sparse painted-cell records in the file.
    readonly painted: boolean[];
    sheet: ImageAsset;
}

export type JsonValue = null | boolean | number | string | readonly JsonValue[]
    | { readonly [key: string]: JsonValue };

export interface ProjectDocument
{
    /// Opaque loaded JSON, shared unchanged by history and save snapshots.
    readonly extensions?: JsonValue;
    gridSize: number;
    propAssets: readonly PropAsset[];
    props: Prop[];
    nextPropId: number;
    layers: Layer[];
    tileSheets: readonly ImageAsset[];
    masks: readonly ImageAsset[];
    nextLayerId: number;
    activeLayerId: number;
    maskMode: "random" | number;
    view: { zoom: number; scrollLeft: number; scrollTop: number; showGrid: boolean };
}

export function createDocument(tiles: readonly ImageAsset[], masks: readonly ImageAsset[]): ProjectDocument
{
    if(tiles.length === 0 || masks.length === 0)
    {
        throw new Error("A new project needs at least one tile sheet and one mask set.");
    }
    const sheets = [tiles[0], tiles[Math.min(1, tiles.length - 1)]];
    const project: ProjectDocument = {
        gridSize: kDefaultGridSize, nextLayerId: 2, activeLayerId: 1,
        propAssets: [], props: [], nextPropId: 0,
        tileSheets: [...tiles], masks: [...masks], maskMode: "random",
        view: { zoom: 1, scrollLeft: 0, scrollTop: 0, showGrid: true },
        layers: sheets.map((sheet, id) => ({
            id, sheet, painted: Array(kDefaultGridSize * kDefaultGridSize).fill(id === 0)
        }))
    };
    validateDocument(project);
    return project;
}

/// Snapshots own their map arrays but share immutable image assets and extensions.
export function cloneDocument(project: ProjectDocument): ProjectDocument
{
    return {
        ...project, propAssets: [...project.propAssets], props: project.props.map((prop) => ({ ...prop })),
        view: { ...project.view }, tileSheets: [...project.tileSheets], masks: [...project.masks],
        layers: project.layers.map((layer) => ({ ...layer, painted: [...layer.painted] }))
    };
}

export function documentBytes(project: ProjectDocument): number
{
    // Conservative estimate for boolean array slots, excluding shared images.
    return project.layers.reduce((total, layer) => total + 128 + layer.painted.length * 8,
        256 + project.props.length * 128 + (project.tileSheets.length + project.masks.length + project.propAssets.length) * 8);
}

/// View changes and swatch selection do not create undo actions.
export function sameContent(a: ProjectDocument, b: ProjectDocument): boolean
{
    if(a.gridSize !== b.gridSize || a.nextPropId !== b.nextPropId || a.props.length !== b.props.length
        || a.propAssets.length !== b.propAssets.length
        || a.propAssets.some((asset, index) => asset !== b.propAssets[index])
        || a.props.some((prop, index) => {
            const other = b.props[index];
            return prop.id !== other.id || prop.asset !== other.asset || prop.x !== other.x
                || prop.y !== other.y || prop.rotation !== other.rotation;
        })
        || a.nextLayerId !== b.nextLayerId || a.maskMode !== b.maskMode
        || a.layers.length !== b.layers.length || a.masks.length !== b.masks.length
        || a.tileSheets.length !== b.tileSheets.length
        || a.tileSheets.some((sheet, index) => sheet !== b.tileSheets[index])
        || a.masks.some((mask, index) => mask !== b.masks[index]))
    {
        return false;
    }
    return a.layers.every((layer, index) => {
        const other = b.layers[index];
        return layer.id === other.id && layer.sheet === other.sheet && layer.painted.length === other.painted.length
            && layer.painted.every((painted, cell) => painted === other.painted[cell]);
    });
}

/// Compare inputs to terrain rendering. Images are immutable and compared by identity.
/// Props, view settings, catalog-only terrain images, and session layer IDs do not affect it.
/// Selected-layer coverage must additionally account for the active layer's array index.
export function sameTerrainContent(a: ProjectDocument, b: ProjectDocument): boolean
{
    return a.gridSize === b.gridSize && a.maskMode === b.maskMode
        && a.layers.length === b.layers.length && a.masks.length === b.masks.length
        && a.masks.every((mask, index) => mask === b.masks[index])
        && a.layers.every((layer, index) => {
            const other = b.layers[index];
            return layer.sheet === other.sheet && layer.painted.length === other.painted.length
                && layer.painted.every((painted, cell) => painted === other.painted[cell]);
        });
}

export function documentAssets(project: ProjectDocument): (ImageAsset | PropAsset)[]
{
    return [...new Set([...project.tileSheets, ...project.masks, ...project.propAssets])];
}

/// Asset IDs contain literal filenames, not URL escapes. Masks remain a flat catalog.
function validAssetPath(asset: ImageAsset | PropAsset): boolean
{
    const root = asset.kind === "tile" ? "tiles" : asset.kind === "mask" ? "masks" : "props";
    const parts = asset.id.split("/");
    for(const character of asset.id)
    {
        if(character.charCodeAt(0) < 32 || character.charCodeAt(0) === 127) { return false; }
    }
    return asset.id.length <= 1024 && parts.length >= 3
        && parts[0] === "assets" && parts[1] === root
        && (asset.kind !== "mask" || parts.length === 3)
        && parts.every((part) => part.length > 0 && part !== "." && part !== ".." && !part.includes("\\"));
}

export function validateDocument(project: ProjectDocument): void
{
    const fail = (message: string): never => { throw new Error(`Invalid project: ${message}`); };
    if(!validGridSize(project.gridSize)
        || project.tileSheets.length < 1 || project.tileSheets.length > 256
        || project.layers.length < 1 || project.layers.length > 256 || project.masks.length < 1
        || project.masks.length > 64)
    {
        fail("unsupported map dimensions or layer/mask count");
    }
    const dimension = project.tileSheets[0].dimension;
    for(const sheet of project.tileSheets)
    {
        if(sheet.kind !== "tile" || sheet.dimension !== dimension)
        {
            fail(`${sheet.label}: terrain textures must all be ${dimension}×${dimension} pixels`);
        }
        if(!Number.isSafeInteger(sheet.dimension) || sheet.dimension < kCellSize
            || sheet.dimension > 4096 || sheet.dimension % kCellSize !== 0)
        {
            fail(`${sheet.label}: terrain textures must be 64–4096 pixels per side and a multiple of 64 pixels`);
        }
    }
    const ids = new Set<number>();
    for(const [index, layer] of project.layers.entries())
    {
        if(!Number.isSafeInteger(layer.id) || layer.id < 0 || ids.has(layer.id))
        {
            fail("layer IDs must be unique nonnegative integers");
        }
        ids.add(layer.id);
        if(!project.tileSheets.includes(layer.sheet))
        {
            fail(`${layer.sheet.label} is not in the terrain texture catalog`);
        }
        if(layer.painted.length !== project.gridSize * project.gridSize || layer.painted.some((value) => typeof value !== "boolean"))
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
    if(project.masks.some((mask) => mask.kind !== "mask" || mask.dimension !== kMaskAtlasPixelSize)
        || (project.maskMode !== "random" && (!Number.isInteger(project.maskMode)
            || project.maskMode < 0 || project.maskMode >= project.masks.length)))
    {
        fail("invalid mask set or selection");
    }
    if(project.propAssets.length > 256 || project.props.length > 10000
        || new Set(project.propAssets).size !== project.propAssets.length)
    {
        fail("invalid prop catalog or instance count");
    }
    for(const asset of project.propAssets)
    {
        if(asset.kind !== "prop"
            || !Number.isSafeInteger(asset.width) || !Number.isSafeInteger(asset.height)
            || asset.width < 1 || asset.height < 1 || asset.width > 4096 || asset.height > 4096
            || !Number.isFinite(asset.pivotX) || asset.pivotX < 0 || asset.pivotX > 1
            || !Number.isFinite(asset.pivotY) || asset.pivotY < 0 || asset.pivotY > 1
            || !Number.isFinite(asset.scale) || asset.scale <= 0)
        {
            fail("invalid prop image");
        }
        const rect = asset.srcRect;
        if(!rect || !Number.isSafeInteger(rect.x) || !Number.isSafeInteger(rect.y)
            || !Number.isSafeInteger(rect.w) || !Number.isSafeInteger(rect.h)
            || rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0
            || rect.x + rect.w > asset.width || rect.y + rect.h > asset.height)
        {
            fail("invalid prop source rectangle");
        }
        if(!Array.isArray(asset.collisionRects) || asset.collisionRects.some((rect) => !rect
            || !Number.isFinite(rect.x) || !Number.isFinite(rect.y)
            || !Number.isFinite(rect.w) || !Number.isFinite(rect.h) || rect.w <= 0 || rect.h <= 0))
        {
            fail("invalid prop collision rectangles");
        }
    }
    const propIds = new Set<number>();
    for(const prop of project.props)
    {
        if(!Number.isSafeInteger(prop.id) || prop.id < 0 || propIds.has(prop.id)
            || !project.propAssets.includes(prop.asset)
            || !Number.isFinite(prop.x) || !Number.isFinite(prop.y)
            || Math.abs(prop.x) > project.gridSize / 2 || Math.abs(prop.y) > project.gridSize / 2
            || !Number.isFinite(prop.rotation) || prop.rotation < 0 || prop.rotation >= 360)
        {
            fail("invalid prop placement");
        }
        propIds.add(prop.id);
    }
    if(!Number.isSafeInteger(project.nextPropId) || project.nextPropId < 0
        || project.props.some((prop) => prop.id >= project.nextPropId))
    {
        fail("invalid next prop ID");
    }
    const assets = documentAssets(project);
    if(new Set(project.tileSheets).size !== project.tileSheets.length
        || new Set(project.masks).size !== project.masks.length
        || new Set(assets.map((asset) => asset.id)).size !== assets.length
        || assets.some((asset) => !validAssetPath(asset)
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
            || (kind === "mask" && dimension !== kMaskAtlasPixelSize)
            || (kind === "tile" && dimension % kCellSize !== 0))
        {
            throw new Error(`${label}: ${kind === "mask" ? "masks must be 256×256" : "terrain textures must be square, at most 4096×4096, and a multiple of 64 pixels"}`);
        }
        const canvas = new OffscreenCanvas(dimension, dimension);
        const context = canvas.getContext("2d");
        if(context === null) { throw new Error("Canvas 2D is unavailable"); }
        context.drawImage(bitmap, 0, 0);
        const pixels = context.getImageData(0, 0, dimension, dimension).data;
        const coverage = new Uint8Array(kind === "mask" ? kMaskAtlasPixelSize * kMaskAtlasPixelSize : 0);
        if(kind === "mask")
        {
            for(let pattern = 0; pattern < kMaskAtlasGridSize * kMaskAtlasGridSize; pattern++)
            {
                for(let y = 0; y < kCellSize; y++)
                {
                    for(let x = 0; x < kCellSize; x++)
                    {
                        coverage[pattern * kMaskPixelCount + y * kCellSize + x] = pixels[
                            ((Math.floor(pattern / kMaskAtlasGridSize) * kCellSize + y) * kMaskAtlasPixelSize
                                + pattern % kMaskAtlasGridSize * kCellSize + x) * 4];
                    }
                }
            }
        }
        return { id, kind, label, blob, dimension, pixels, coverage, url: URL.createObjectURL(blob) };
    }
    finally { bitmap.close(); }
}

/// Keep a decoded canvas with each immutable asset so drawing never waits for image loading.
export async function decodePropAsset(blob: Blob, label: string, id: string): Promise<PropAsset>
{
    let bitmap: ImageBitmap;
    try { bitmap = await createImageBitmap(blob); }
    catch { throw new Error(`Could not decode ${label}`); }
    try
    {
        const { width, height } = bitmap;
        if(width < 1 || height < 1 || width > 4096 || height > 4096)
        {
            throw new Error(`${label}: prop images must be 1–4096 pixels along each axis`);
        }
        const image = new OffscreenCanvas(width, height);
        const context = image.getContext("2d");
        if(context === null) { throw new Error("Canvas 2D is unavailable"); }
        context.drawImage(bitmap, 0, 0);
        return { id, label, blob, url: URL.createObjectURL(blob), kind: "prop", width, height, image,
            srcRect: { x: 0, y: 0, w: width, h: height }, pivotX: 0.5, pivotY: 0.5, scale: 1, collisionRects: [] };
    }
    finally { bitmap.close(); }
}

import { kMinGridSize, kMaxGridSize, validGridSize } from "./grid.ts";
import { validateDocument, propName, propTint, type ImageAsset, type PropAsset, type ProjectDocument, type JsonValue } from "./document.ts";

/// Serialize occupancy and rendering settings. Texture and mask choices are derived.
export function writeProject(project: ProjectDocument): Promise<Blob>
{
    validateDocument(project);
    const blob = new Blob([JSON.stringify({
        format: "mosaic", version: 24,
        gridSize: project.gridSize,
        tileSheets: project.tileSheets.map((sheet) => "/" + sheet.id),
        maskSheets: project.masks.map((sheet) => "/" + sheet.id),
        maskMode: project.maskMode,
        propAssets: project.propAssets.map((asset) => ({
            name: propName(asset), size: asset.size, tint: propTint(asset),
            texPath: "/" + asset.id, pivot: { x: asset.pivotX, y: asset.pivotY }, scale: asset.scale,
            srcRect: asset.srcRect.x === 0 && asset.srcRect.y === 0
                && asset.srcRect.w === asset.width && asset.srcRect.h === asset.height ? {} : { ...asset.srcRect },
            collisionRects: asset.collisionRects.map(({ x, y, w, h }) => ({ x, y, w, h }))
        })),
        props: project.props.map((prop) => ({
            index: project.propAssets.indexOf(prop.asset),
            pos: { x: Math.round(prop.x * 1000) / 1000, y: Math.round(prop.y * 1000) / 1000 },
            rot: prop.rotation
        })),
        layers: project.layers.map((layer, index) => ({
            sheet: project.tileSheets.indexOf(layer.sheet),
            cells: index === 0 ? [] : layer.painted.flatMap((painted, cell) => painted ? [cell] : [])
        })),
        extensions: project.extensions,
        editor: {
            activeLayer: project.layers.findIndex((layer) => layer.id === project.activeLayerId),
            view: project.view
        }
    }, null, 2) + "\n"], { type: "application/json" });
    return Promise.resolve(blob);
}

function object(value: unknown): Record<string, unknown>
{
    if(typeof value !== "object" || value === null || Array.isArray(value))
    {
        throw new Error("Invalid project object");
    }
    return value as Record<string, unknown>;
}

function integer(value: unknown, min: number, max: number): number
{
    if(typeof value !== "number" || !Number.isSafeInteger(value) || value < min || value > max)
    {
        throw new Error("Invalid project integer");
    }
    return value;
}

/// File paths are rooted at the project. Discovery uses IDs relative to that root.
function assetId(value: unknown): string
{
    if(typeof value !== "string" || !value.startsWith("/assets/") || value.length > 1025)
    {
        throw new Error("Invalid project asset path");
    }
    const id = value.slice(1);
    if(id.split("/").some((part) => !part || part === "." || part === "..")
        || id.includes("\\") || [...id].some((character) => character.charCodeAt(0) < 32 || character.charCodeAt(0) === 127))
    {
        throw new Error("Invalid project asset path");
    }
    return id;
}

function list(value: unknown, min: number, max: number): unknown[]
{
    if(!Array.isArray(value) || value.length < min || value.length > max)
    {
        throw new Error("Invalid project list");
    }
    return value;
}

function numberOrDefault(value: unknown, fallback: number): number
{
    if(value === undefined) { return fallback; }
    if(typeof value !== "number" || !Number.isFinite(value)) { throw new Error("Invalid project number"); }
    return value;
}

/// Explicit texture references may live anywhere beneath the project's assets directory.
export async function projectTexturePaths(blob: Blob): Promise<string[]>
{
    const data = object(JSON.parse(await blob.text()));
    if(data.format !== "mosaic" || data.version !== 24) { throw new Error("Unsupported Mosaic project format/version"); }
    return [...new Set(list(data.propAssets, 0, 256).map((entry) => assetId(object(entry).texPath)))];
}

export async function readProject(blob: Blob, available: readonly (ImageAsset | PropAsset)[]): Promise<ProjectDocument>
{
    const data = object(JSON.parse(await blob.text()));
    if(data.format !== "mosaic" || data.version !== 24)
    {
        throw new Error("Unsupported Mosaic project format/version");
    }
    const editor = object(data.editor);
    const assets = new Map(available.filter((asset) => asset.kind !== "prop").map((asset) => [asset.id, asset]));
    const propImages = new Map(available.filter((asset) => asset.kind === "prop").map((asset) => [asset.id, asset]));
    const lookup = (id: unknown): ImageAsset | PropAsset => {
        const asset = assets.get(assetId(id));
        if(!asset) { throw new Error(`Missing image: ${id}`); }
        return asset;
    };
    const terrainAsset = (id: unknown): ImageAsset => {
        const asset = lookup(id);
        if(asset.kind === "prop") { throw new Error("Expected terrain or mask image"); }
        return asset;
    };
    const tileSheets = list(data.tileSheets, 1, 256).map(terrainAsset);
    const masks = list(data.maskSheets, 1, 64).map(terrainAsset);
    const propAssets = list(data.propAssets, 0, 256).map((item) => {
        const entry = object(item);
        const asset = propImages.get(assetId(entry.texPath));
        if(!asset) { throw new Error(`Missing image: ${entry.texPath}`); }
        if(asset.kind !== "prop") { throw new Error("Expected prop image"); }
        const savedRect = entry.srcRect === undefined ? {} : object(entry.srcRect);
        const rect = Object.keys(savedRect).length === 0
            ? { x: 0, y: 0, w: asset.width, h: asset.height } : savedRect;
        const srcRect = { x: integer(rect.x, 0, asset.width - 1), y: integer(rect.y, 0, asset.height - 1),
            w: integer(rect.w, 1, asset.width), h: integer(rect.h, 1, asset.height) };
        const pivot = entry.pivot === undefined ? {} : object(entry.pivot);
        if(entry.collisionRects !== undefined && !Array.isArray(entry.collisionRects)) { throw new Error("Invalid prop collision rectangle"); }
        const collisionRects = ((entry.collisionRects ?? []) as unknown[]).map((value) => {
            const rect = object(value);
            for(const key of ["x", "y", "w", "h"])
            {
                if(typeof rect[key] !== "number" || !Number.isFinite(rect[key]))
                {
                    throw new Error("Invalid prop collision rectangle component");
                }
            }
            return { x: rect.x as number, y: rect.y as number, w: rect.w as number, h: rect.h as number };
        });
        const name = entry.name === undefined ? propName(asset) : entry.name;
        if(typeof name !== "string") { throw new Error("Invalid sequence name"); }
        const size = entry.size === undefined ? undefined : object(entry.size);
        const frameSize = size === undefined ? undefined
            : { w: integer(size.w, 1, srcRect.w), h: integer(size.h, 1, srcRect.h) };
        const tintData = entry.tint === undefined ? { r: 1, g: 1, b: 1, a: 1 } : object(entry.tint);
        for(const key of ["r", "g", "b", "a"])
        {
            const value = tintData[key];
            if(typeof value !== "number" || !Number.isFinite(value) || value < 0 || value > 1)
            {
                throw new Error("Invalid sequence tint");
            }
        }
        const tint = { r: tintData.r as number, g: tintData.g as number, b: tintData.b as number, a: tintData.a as number };
        // Keep decoded images shared without changing metadata in another document.
        return { ...asset, name, size: frameSize, tint, srcRect,
            pivotX: numberOrDefault(pivot.x, 0.5),
            pivotY: numberOrDefault(pivot.y, 0.5),
            scale: numberOrDefault(entry.scale, 1), collisionRects };
    });
    const props = list(data.props, 0, 10000).map((item, id) => {
        const prop = object(item);
        const pos = object(prop.pos);
        if(typeof pos.x !== "number" || typeof pos.y !== "number" || typeof prop.rot !== "number")
        {
            throw new Error("Invalid prop coordinate or rotation");
        }
        return { id, asset: propAssets[integer(prop.index, 0, propAssets.length - 1)],
            x: pos.x, y: pos.y, rotation: prop.rot };
    });
    const gridSize = integer(data.gridSize, kMinGridSize, kMaxGridSize);
    if(!validGridSize(gridSize)) { throw new Error("Map size must be a power of two from 2 to 512 cells"); }
    const cellCount = gridSize * gridSize;
    const layers = list(data.layers, 1, 256);
    const project: ProjectDocument = {
        extensions: data.extensions as JsonValue | undefined,
        gridSize,
        nextLayerId: layers.length,
        propAssets, props, nextPropId: props.length,
        activeLayerId: integer(editor.activeLayer, 0, layers.length - 1),
        maskMode: data.maskMode === "random" ? "random" : integer(data.maskMode, 0, masks.length - 1),
        tileSheets, masks, view: object(editor.view) as ProjectDocument["view"],
        layers: layers.map((item, index) => {
            const layer = object(item);
            const sheet = tileSheets[integer(layer.sheet, 0, tileSheets.length - 1)];
            const painted: boolean[] = Array(cellCount).fill(index === 0);
            for(const value of list(layer.cells, 0, index === 0 ? 0 : cellCount))
            {
                const cell = integer(value, 0, cellCount - 1);
                if(painted[cell]) { throw new Error("Duplicate painted cell"); }
                painted[cell] = true;
            }
            return {
                id: index, sheet, painted
            };
        })
    };
    validateDocument(project);
    // Preserve catalog indices. Additional mask sheets affect random selection.
    project.tileSheets = [...tileSheets, ...available.filter((asset): asset is ImageAsset =>
        asset.kind === "tile" && !tileSheets.includes(asset))];
    project.masks = [...masks, ...available.filter((asset): asset is ImageAsset =>
        asset.kind === "mask" && !masks.includes(asset))];
    project.propAssets = [...propAssets, ...available.filter((asset): asset is PropAsset =>
        asset.kind === "prop" && !propAssets.some((saved) => saved.id === asset.id))];
    validateDocument(project);
    // Validate the original coordinates before rounding so invalid bounds still fail.
    for(const prop of project.props)
    {
        prop.x = Math.round(prop.x * 1000) / 1000;
        prop.y = Math.round(prop.y * 1000) / 1000;
    }
    return project;
}

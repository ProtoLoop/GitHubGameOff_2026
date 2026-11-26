import { kMinGridSize, kMaxGridSize, validGridSize } from "./grid.ts";
import { validateDocument, appendDiscoveredSequences, sequenceName, sequenceTint, type ImageAsset, type SpriteSequence, type ProjectDocument, type JsonValue, type ObjectDefinition, type Placement } from "./document.ts";

/// Format plain JSON data, keeping small scalar-only objects on one line.
function formatJson(value: unknown, depth: number, column: number): string
{
    if(value === null || typeof value !== "object") { return JSON.stringify(value); }
    const indent = "  ".repeat(depth);
    const childIndent = indent + "  ";
    if(Array.isArray(value))
    {
        if(value.length === 0) { return "[]"; }
        return "[\n" + value.map((item) => childIndent + formatJson(item, depth + 1, childIndent.length))
            .join(",\n") + "\n" + indent + "]";
    }
    const entries = Object.entries(value).filter(([, item]) => item !== undefined);
    if(entries.length === 0) { return "{}"; }
    if(entries.every(([, item]) => item === null || typeof item !== "object"))
    {
        const compact = JSON.stringify(value);
        if(column + compact.length <= 120) { return compact; }
    }
    return "{\n" + entries.map(([key, item]) => {
        const prefix = childIndent + JSON.stringify(key) + ": ";
        return prefix + formatJson(item, depth + 1, prefix.length);
    }).join(",\n") + "\n" + indent + "}";
}

function placementTransform(placement: Placement): { pos: { x: number; y: number }; rot: number; scale?: number }
{
    return { pos: { x: Math.round(placement.x * 1000) / 1000, y: Math.round(placement.y * 1000) / 1000 }, rot: placement.rotation, scale: placement.scale };
}

/// Serialize occupancy and rendering settings. Texture and mask choices are derived.
export function writeProject(project: ProjectDocument): Promise<Blob>
{
    validateDocument(project);
    const blob = new Blob([formatJson({
        format: "mosaic", version: 29,
        gridSize: project.gridSize,
        tileSheets: project.tileSheets.map((sheet) => "/" + sheet.id),
        maskSheets: project.masks.map((sheet) => "/" + sheet.id),
        maskMode: project.maskMode,
        sequences: project.sequences.map((asset) => ({
            name: sequenceName(asset), size: asset.size, tint: sequenceTint(asset),
            texPath: "/" + asset.id, pivot: { x: asset.pivotX, y: asset.pivotY }, scale: asset.scale,
            srcRect: asset.srcRect.x === 0 && asset.srcRect.y === 0
                && asset.srcRect.w === asset.width && asset.srcRect.h === asset.height ? {} : { ...asset.srcRect },
            collisionRects: asset.collisionRects.map(({ x, y, w, h }) => ({ x, y, w, h }))
        })),
        sprites: project.sprites.map((sprite) => ({ name: sprite.name, sequences: sprite.sequences.map(sequenceName) })),
        propTypes: project.propTypes.map((definition) => ({ name: definition.name, sprite: definition.sprite.name })),
        actorTypes: project.actorTypes.map((definition) => ({ name: definition.name, class: definition.class, sprite: definition.sprite.name })),
        props: project.placements.filter((placement) => placement.definition.kind === "prop").map((placement) => ({
            type: placement.definition.name, ...placementTransform(placement)
        })),
        actors: project.placements.filter((placement) => placement.definition.kind === "actor").map((placement) => ({
            type: placement.definition.name, ...placementTransform(placement)
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
    }, 0, 0) + "\n"], { type: "application/json" });
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
    if(data.format !== "mosaic" || data.version !== 29) { throw new Error("Unsupported Mosaic project format/version"); }
    return [...new Set(list(data.sequences, 0, 256).map((entry) => assetId(object(entry).texPath)))];
}

export async function readProject(blob: Blob, available: readonly (ImageAsset | SpriteSequence)[]): Promise<ProjectDocument>
{
    const data = object(JSON.parse(await blob.text()));
    if(data.format !== "mosaic" || data.version !== 29)
    {
        throw new Error("Unsupported Mosaic project format/version");
    }
    const editor = object(data.editor);
    const assets = new Map(available.filter((asset) => asset.kind !== "prop").map((asset) => [asset.id, asset]));
    const propImages = new Map(available.filter((asset) => asset.kind === "prop").map((asset) => [asset.id, asset]));
    const lookup = (id: unknown): ImageAsset | SpriteSequence => {
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
    const sequences = list(data.sequences, 0, 256).map((item) => {
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
        const name = entry.name === undefined ? sequenceName(asset) : entry.name;
        if(typeof name !== "string") { throw new Error("Invalid sequence name"); }
        const size = entry.size === undefined ? undefined : object(entry.size);
        const frameSize = size === undefined ? undefined
            : { w: integer(size.w, 1, srcRect.w), h: integer(size.h, 1, srcRect.h) };
        const tintData = entry.tint === undefined ? { r: 1, g: 1, b: 1, a: 1 } : object(entry.tint);
        for(const key of ["r", "g", "b", "a"])
        {
            const value = tintData[key];
            if(typeof value !== "number" || !Number.isFinite(value) || value < 0)
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
    const sequenceNames = new Map(sequences.map((sequence) => [sequence.name, sequence]));
    const sprites = list(data.sprites, 0, 256).map((item) => {
        const entry = object(item);
        if(typeof entry.name !== "string") { throw new Error("Invalid sprite name"); }
        return { name: entry.name, sequences: list(entry.sequences, 1, 256).map((name) => {
            if(typeof name !== "string") { throw new Error("Invalid sequence reference"); }
            const sequence = sequenceNames.get(name);
            if(!sequence) { throw new Error(`Missing sequence: ${name}`); }
            return sequence;
        }) };
    });
    const spriteNames = new Map(sprites.map((sprite) => [sprite.name, sprite]));
    const readDefinition = (item: unknown): { name: string; sprite: typeof sprites[number] } => {
        const entry = object(item);
        if(typeof entry.name !== "string" || typeof entry.sprite !== "string") { throw new Error("Invalid object definition"); }
        const sprite = spriteNames.get(entry.sprite);
        if(!sprite) { throw new Error(`Missing sprite: ${entry.sprite}`); }
        return { name: entry.name, sprite };
    };
    const propTypes = list(data.propTypes, 0, 256).map((item) => ({ kind: "prop" as const, ...readDefinition(item) }));
    const actorTypes = list(data.actorTypes, 0, 256).map((item) => {
        const entry = object(item);
        if(typeof entry.class !== "string") { throw new Error("Invalid actor class"); }
        return { kind: "actor" as const, class: entry.class, ...readDefinition(item) };
    });
    let nextPlacementId = 0;
    const readPlacements = (items: unknown, kind: "prop" | "actor", definitions: readonly ObjectDefinition[]): Placement[] => {
        const names = new Map(definitions.map((definition) => [definition.name, definition]));
        return list(items, 0, 10000).map((item) => {
            const entry = object(item);
            const pos = object(entry.pos);
            if(typeof pos.x !== "number" || typeof pos.y !== "number" || typeof entry.rot !== "number")
            {
                throw new Error(`Invalid ${kind} coordinate or rotation`);
            }
            const name = entry.type;
            if(typeof name !== "string") { throw new Error(`Invalid ${kind} reference`); }
            const definition = names.get(name);
            if(!definition) { throw new Error(`Missing ${kind} definition: ${name}`); }
            if(entry.scale !== undefined && (typeof entry.scale !== "number" || !Number.isFinite(entry.scale) || entry.scale <= 0))
            {
                throw new Error(`Invalid ${kind} scale`);
            }
            return { id: nextPlacementId++, definition, x: pos.x, y: pos.y, rotation: entry.rot,
                ...(entry.scale === undefined ? {} : { scale: entry.scale as number }) };
        });
    };
    const placements = [...readPlacements(data.props, "prop", propTypes), ...readPlacements(data.actors, "actor", actorTypes)];
    const gridSize = integer(data.gridSize, kMinGridSize, kMaxGridSize);
    if(!validGridSize(gridSize)) { throw new Error("Map size must be a power of two from 2 to 512 cells"); }
    const cellCount = gridSize * gridSize;
    const layers = list(data.layers, 1, 256);
    const project: ProjectDocument = {
        extensions: data.extensions as JsonValue | undefined,
        gridSize,
        nextLayerId: layers.length,
        sequences, sprites, propTypes, actorTypes, placements, nextPlacementId,
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
    appendDiscoveredSequences(project, available.filter((asset): asset is SpriteSequence => asset.kind === "prop"));
    validateDocument(project);
    // Validate the original coordinates before rounding so invalid bounds still fail.
    for(const prop of project.placements)
    {
        prop.x = Math.round(prop.x * 1000) / 1000;
        prop.y = Math.round(prop.y * 1000) / 1000;
    }
    return project;
}

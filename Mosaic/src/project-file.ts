import { kGridSize, kCellCount } from "./grid.ts";
import { validateDocument, type ImageAsset, type PropAsset, type ProjectDocument } from "./document.ts";

/// Serialize occupancy and rendering settings. Texture and mask choices are derived.
export function writeProject(project: ProjectDocument): Promise<Blob>
{
    validateDocument(project);
    const blob = new Blob([JSON.stringify({
        format: "mosaic", version: 9,
        gridSize: project.gridSize,
        tileSheets: project.tileSheets.map((sheet) => sheet.id),
        maskSheets: project.masks.map((sheet) => sheet.id),
        maskMode: project.maskMode,
        propAssets: project.propAssets.map((asset) => asset.id),
        props: project.props.map((prop) => ({
            asset: project.propAssets.indexOf(prop.asset), x: prop.x, y: prop.y, rotation: prop.rotation
        })),
        layers: project.layers.map((layer, index) => ({
            sheet: project.tileSheets.indexOf(layer.sheet),
            cells: index === 0 ? [] : layer.painted.flatMap((painted, cell) => painted ? [cell] : [])
        })),
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

function text(value: unknown): string
{
    if(typeof value !== "string" || value.length > 1024) { throw new Error("Invalid project text"); }
    return value;
}

function list(value: unknown, min: number, max: number): unknown[]
{
    if(!Array.isArray(value) || value.length < min || value.length > max)
    {
        throw new Error("Invalid project list");
    }
    return value;
}

export async function readProject(blob: Blob, available: readonly (ImageAsset | PropAsset)[]): Promise<ProjectDocument>
{
    const data = object(JSON.parse(await blob.text()));
    if(data.format !== "mosaic" || data.version !== 9)
    {
        throw new Error("Unsupported Mosaic project format/version");
    }
    const editor = object(data.editor);
    const assets = new Map(available.map((asset) => [asset.id, asset]));
    const lookup = (id: unknown): ImageAsset | PropAsset => {
        const asset = assets.get(text(id));
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
    const propAssets = list(data.propAssets, 0, 256).map((id) => {
        const asset = lookup(id);
        if(asset.kind !== "prop") { throw new Error("Expected prop image"); }
        return asset;
    });
    const props = list(data.props, 0, 10000).map((item, id) => {
        const prop = object(item);
        for(const key of ["x", "y", "rotation"])
        {
            if(typeof prop[key] !== "number") { throw new Error("Invalid prop coordinate or rotation"); }
        }
        return { id, asset: propAssets[integer(prop.asset, 0, propAssets.length - 1)],
            x: prop.x as number, y: prop.y as number, rotation: prop.rotation as number };
    });
    const layers = list(data.layers, 1, 256);
    const project: ProjectDocument = {
        gridSize: integer(data.gridSize, kGridSize, kGridSize) as typeof kGridSize,
        nextLayerId: layers.length,
        propAssets, props, nextPropId: props.length,
        activeLayerId: integer(editor.activeLayer, 0, layers.length - 1),
        maskMode: data.maskMode === "random" ? "random" : integer(data.maskMode, 0, masks.length - 1),
        tileSheets, masks, view: object(editor.view) as ProjectDocument["view"],
        layers: layers.map((item, index) => {
            const layer = object(item);
            const sheet = tileSheets[integer(layer.sheet, 0, tileSheets.length - 1)];
            const painted: boolean[] = Array(kCellCount).fill(index === 0);
            for(const value of list(layer.cells, 0, index === 0 ? 0 : kCellCount))
            {
                const cell = integer(value, 0, kCellCount - 1);
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
        asset.kind === "prop" && !propAssets.includes(asset))];
    validateDocument(project);
    return project;
}

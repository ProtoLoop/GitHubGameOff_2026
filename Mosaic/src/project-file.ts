import { kGridSize, kCellSize, kCellCount } from "./grid.ts";
import { validateDocument, type ImageAsset, type ProjectDocument } from "./document.ts";

/// Serialize occupancy and rendering settings. Texture and mask choices are derived.
export function writeProject(project: ProjectDocument): Promise<Blob>
{
    validateDocument(project);
    const blob = new Blob([JSON.stringify({
        format: "mosaic", version: 6,
        gridSize: project.gridSize, tileSize: project.tileSize,
        tileSheets: project.tileSheets.map((sheet) => sheet.id),
        maskSheets: project.masks.map((sheet) => sheet.id),
        maskMode: project.maskMode,
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

export async function readProject(blob: Blob, available: readonly ImageAsset[]): Promise<ProjectDocument>
{
    const data = object(JSON.parse(await blob.text()));
    if(data.format !== "mosaic" || data.version !== 6)
    {
        throw new Error("Unsupported Mosaic project format/version");
    }
    const editor = object(data.editor);
    const assets = new Map(available.map((asset) => [asset.id, asset]));
    const lookup = (id: unknown): ImageAsset => {
        const asset = assets.get(text(id));
        if(!asset) { throw new Error(`Missing image: ${id}`); }
        return asset;
    };
    const tileSize = integer(data.tileSize, kCellSize, 4096);
    const tileSheets = list(data.tileSheets, 1, 256).map(lookup);
    const masks = list(data.maskSheets, 1, 64).map(lookup);
    const layers = list(data.layers, 1, 256);
    const project: ProjectDocument = {
        gridSize: integer(data.gridSize, kGridSize, kGridSize) as typeof kGridSize,
        tileSize, nextLayerId: layers.length,
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
    project.tileSheets = [...tileSheets, ...available.filter((asset) =>
        asset.kind === "tile" && !tileSheets.includes(asset))];
    project.masks = [...masks, ...available.filter((asset) =>
        asset.kind === "mask" && !masks.includes(asset))];
    validateDocument(project);
    return project;
}

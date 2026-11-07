import { listDirectory, loadProject } from "./project-http.ts";
import { atlasGrid, documentAssets,
    validateDocument, type ProjectDocument, type Layer, type ImageAsset as TileAtlas } from "./document.ts";
import { History } from "./history.ts";
import { writeProject } from "./project-file.ts";
import { automaticTile, cellMasks, sampleMask } from "./placement.ts";
import { ProjectSession, type SaveStatus } from "./project-session.ts";

const kGridSize = 64;
const kCellSize = 64;
const kHalfCellSize = kCellSize / 2;
let projectMasks: readonly TileAtlas[] = [];
let project: ProjectDocument;
let ready = false;
let fileBusy = false;
const assets = new Set<TileAtlas>();
const editHistory = new History(100, 64 * 1024 * 1024);
let tileAtlases: readonly TileAtlas[] = [];
let paintPointerId: number | null = null;
let lastPaintCell = -1;
let panPointerId: number | null = null;
let panX = 0;
let panY = 0;

interface Controls
{
    readonly map: HTMLCanvasElement;
    readonly mapViewport: HTMLDivElement;
    readonly grid: SVGSVGElement;
    readonly coverage: HTMLCanvasElement;
    readonly status: HTMLOutputElement;
    readonly layers: HTMLDivElement;
    readonly palette: HTMLDivElement;
    readonly addLayer: HTMLButtonElement;
    readonly activeMask: HTMLSelectElement;
    readonly showGrid: HTMLInputElement;
    readonly mapTileSize: HTMLInputElement;
}

function required<T extends Element>(id: string, type: { new(): T }): T
{
    const node = document.getElementById(id);
    if(!(node instanceof type))
    {
        throw new Error(`Missing ${id} element`);
    }
    return node;
}

const controls: Controls = {
    map : required("map", HTMLCanvasElement),
    mapViewport : required("map-viewport", HTMLDivElement),
    grid : required("map-grid", SVGSVGElement),
    coverage : required("coverage", HTMLCanvasElement),
    status : required("status", HTMLOutputElement),
    layers : required("layers", HTMLDivElement),
    palette : required("material-palette", HTMLDivElement),
    addLayer : required("add-layer", HTMLButtonElement),
    activeMask : required("active-mask", HTMLSelectElement),
    showGrid : required("show-grid", HTMLInputElement),
    mapTileSize : required("map-tile-size", HTMLInputElement)
};

function getLayer(id: number): Layer
{
    const layer = project.layers.find((candidate) => candidate.id === id);
    if(layer === undefined)
    {
        throw new Error(`Missing layer ${id}`);
    }
    return layer;
}

function gridSize(layer: Layer, tileSize: number): number
{
    if(layer.sheet === null || tileSize < 1 || layer.sheet.dimension % tileSize !== 0)
    {
        return 0;
    }
    const count = layer.sheet.dimension / tileSize;
    return count >= 2 && count % 2 === 0 ? count : 0;
}

function sheetFitsMap(layer: Layer, tileSize: number): boolean
{
    return gridSize(layer, tileSize) > 0;
}

function setLayerTileSize(layer: Layer, tileSize: number): void
{
    layer.tileSize = tileSize;
    layer.atlasGrid = gridSize(layer, tileSize);
}

function tileColor(atlas: TileAtlas | null,
    layer: Layer,
    tile: number,
    x: number,
    y: number): readonly [number, number, number]
{
    if(atlas === null || layer.atlasGrid === 0 || tile < 0 || tile >= layer.atlasGrid * layer.atlasGrid)
    {
        return [70, 70, 70];
    }
    const tileX = (tile % layer.atlasGrid) * project.tileSize;
    const tileY = Math.floor(tile / layer.atlasGrid) * project.tileSize;
    const sampleX = Math.min(project.tileSize - 1, Math.floor(x * project.tileSize / kCellSize));
    const sampleY = Math.min(project.tileSize - 1, Math.floor(y * project.tileSize / kCellSize));
    const offset = ((tileY + sampleY) * atlas.dimension + tileX + sampleX) * 4;
    return [atlas.pixels[offset], atlas.pixels[offset + 1], atlas.pixels[offset + 2]];
}

/// Render a rectangle of whole cells. Painting includes the one-cell border
/// touched by the four junction masks whose patterns may have changed.
/// Other pixels remain untouched.
function drawTerrainRegion(startX: number, startY: number, endX: number, endY: number): void
{
    const size = kGridSize * kCellSize;
    const canvas = controls.map;
    if(canvas.width !== size || canvas.height !== size)
    {
        canvas.width = size;
        canvas.height = size;
    }
    const context = canvas.getContext("2d");
    if(context === null)
    {
        throw new Error("Canvas 2D is unavailable");
    }
    const width = (endX - startX) * kCellSize;
    const height = (endY - startY) * kCellSize;
    const image = context.createImageData(width, height);
    const overlays = project.layers.slice(1);
    // Resample each material tile and flip combination once per redraw.
    const textures = new Map<Layer, Map<number, Uint8ClampedArray>>();
    const texture = (layer: Layer, tile: number, hflip: boolean, vflip: boolean): Uint8ClampedArray => {
        let tiles = textures.get(layer);
        if(tiles === undefined)
        {
            tiles = new Map();
            textures.set(layer, tiles);
        }
        const key = tile * 4 + Number(hflip) + 2 * Number(vflip);
        let pixels = tiles.get(key);
        if(pixels === undefined)
        {
            pixels = new Uint8ClampedArray(kCellSize * kCellSize * 3);
            for(let y = 0; y < kCellSize; y++)
            {
                for(let x = 0; x < kCellSize; x++)
                {
                    const sourceX = hflip ? kCellSize - 1 - x : x;
                    const sourceY = vflip ? kCellSize - 1 - y : y;
                    pixels.set(tileColor(layer.sheet, layer, tile, sourceX, sourceY), (y * kCellSize + x) * 3);
                }
            }
            tiles.set(key, pixels);
        }
        return pixels;
    };
    for(let tileY = startY; tileY < endY; tileY++)
    {
        for(let tileX = startX; tileX < endX; tileX++)
        {
            const cell = tileY * kGridSize + tileX;
            const baseTile = automaticTile(project, project.layers[0], cell);
            const base = texture(project.layers[0], baseTile.tile, baseTile.hflip, baseTile.vflip);
            const layers = overlays.map((layer) => {
                const tile = automaticTile(project, layer, cell);
                return {
                    pixels: texture(layer, tile.tile, tile.hflip, tile.vflip), masks: cellMasks(project, layer, cell)
                };
            });
            for(let y = 0; y < kCellSize; y++)
            {
                for(let x = 0; x < kCellSize; x++)
                {
                    const source = (y * kCellSize + x) * 3;
                    const offset = (((tileY - startY) * kCellSize + y) * width
                        + (tileX - startX) * kCellSize + x) * 4;
                    image.data[offset] = base[source];
                    image.data[offset + 1] = base[source + 1];
                    image.data[offset + 2] = base[source + 2];
                    image.data[offset + 3] = 255;
                    const quadrant = (y >= kHalfCellSize ? 2 : 0) + (x >= kHalfCellSize ? 1 : 0);
                    const maskX = (x + kHalfCellSize) % kCellSize;
                    const maskY = (y + kHalfCellSize) % kCellSize;
                    for(const layer of layers)
                    {
                        const alpha = sampleMask(layer.masks[quadrant], project.masks, maskX, maskY);
                        for(let channel = 0; channel < 3; channel++)
                        {
                            image.data[offset + channel] = Math.round((image.data[offset + channel] * (255 - alpha)
                                + layer.pixels[source + channel] * alpha) / 255);
                        }
                    }
                }
            }
        }
    }
    const originX = startX * kCellSize;
    const originY = startY * kCellSize;
    context.putImageData(image, originX, originY);
}

/// Draw only the selected layer's alpha, retaining full mask resolution.
function drawCoverageRegion(startX: number, startY: number, endX: number, endY: number): void
{
    const size = kGridSize * kCellSize;
    const canvas = controls.coverage;
    if(canvas.width !== size || canvas.height !== size)
    {
        canvas.width = size;
        canvas.height = size;
    }
    const context = canvas.getContext("2d");
    if(context === null)
    {
        throw new Error("Canvas 2D is unavailable");
    }
    const width = (endX - startX) * kCellSize;
    const height = (endY - startY) * kCellSize;
    const coverage = context.createImageData(width, height);
    coverage.data.fill(255);
    const selected = project.layers.findIndex((layer) => layer.id === project.activeLayerId);
    if(selected !== 0)
    {
        for(let tileY = startY; tileY < endY; tileY++)
        {
            for(let tileX = startX; tileX < endX; tileX++)
            {
                const cell = tileY * kGridSize + tileX;
                const layer = project.layers[selected];
                const masks = cellMasks(project, layer, cell);
                for(let y = 0; y < kCellSize; y++)
                {
                    for(let x = 0; x < kCellSize; x++)
                    {
                        const offset = (((tileY - startY) * kCellSize + y) * width
                            + (tileX - startX) * kCellSize + x) * 4;
                        const quadrant = (y >= kHalfCellSize ? 2 : 0) + (x >= kHalfCellSize ? 1 : 0);
                        const maskX = (x + kHalfCellSize) % kCellSize;
                        const maskY = (y + kHalfCellSize) % kCellSize;
                        coverage.data[offset + 3] = sampleMask(masks[quadrant], project.masks, maskX, maskY);
                    }
                }
            }
        }
    }
    context.putImageData(coverage, startX * kCellSize, startY * kCellSize);
}

function drawRegion(startX: number, startY: number, endX: number, endY: number): void
{
    drawTerrainRegion(startX, startY, endX, endY);
    drawCoverageRegion(startX, startY, endX, endY);
}

function draw(): void
{
    drawRegion(0, 0, kGridSize, kGridSize);
}

/// Keep the grid vector-based so terrain scaling cannot discard thin lines.
function initializeGrid(): void
{
    const size = kGridSize * kCellSize;
    controls.grid.setAttribute("viewBox", `0 0 ${size} ${size}`);
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    const segments: string[] = [];
    for(let cell = 0; cell <= kGridSize; cell++)
    {
        const position = cell * kCellSize;
        segments.push(`M ${position} 0 V ${size}`, `M 0 ${position} H ${size}`);
    }
    path.setAttribute("d", segments.join(" "));
    path.setAttribute("vector-effect", "non-scaling-stroke");
    controls.grid.replaceChildren(path);
    updateGrid();
}

/// Fade crowded lines below twelve display pixels per cell, hiding at four.
function updateGrid(): void
{
    const fade = Math.max(0, Math.min(1, (kCellSize * project.view.zoom - 4) / 8));
    controls.grid.style.opacity = String(controls.showGrid.checked ? 0.58 * fade : 0);
}

function cellAt(event: PointerEvent): number
{
    const bounds = controls.map.getBoundingClientRect();
    const tileX = Math.floor((event.clientX - bounds.left) * kGridSize / bounds.width);
    const tileY = Math.floor((event.clientY - bounds.top) * kGridSize / bounds.height);
    if(tileX < 0 || tileX >= kGridSize || tileY < 0 || tileY >= kGridSize)
    {
        return -1;
    }
    return tileY * kGridSize + tileX;
}

function paintCell(cell: number, erase: boolean): void
{
    const tileX = cell % kGridSize;
    const tileY = Math.floor(cell / kGridSize);
    const layer = getLayer(project.activeLayerId);
    if(erase && layer === project.layers[0])
    {
        controls.status.value = "The base cannot be cleared. Left-click to reveal it.";
        return;
    }
    if(!erase && layer !== project.layers[0] && !sheetFitsMap(layer, project.tileSize))
    {
        controls.status.value = "Choose a compatible sheet and shared tile size before painting.";
        return;
    }
    if(erase)
    {
        layer.painted[cell] = false;
    }
    else
    {
        layer.painted[cell] = true;
        const layerIndex = project.layers.indexOf(layer);
        for(let index = layerIndex + 1; index < project.layers.length; index++)
        {
            const higher = project.layers[index];
            higher.painted[cell] = false;
        }
    }
    controls.status.value = erase
        ? `${layer.sheet.label}: cleared ${tileX + 1}, ${tileY + 1}`
        : `${layer.sheet.label}: ${layer === project.layers[0] ? "revealed" : "occupied"} ${tileX + 1}, ${tileY + 1}; higher layers cleared`;
    drawRegion(Math.max(0, tileX - 1), Math.max(0, tileY - 1),
        Math.min(kGridSize, tileX + 2), Math.min(kGridSize, tileY + 2));
}

/// Fill intermediate cells when pointer events skip across the grid.
function paintDragAt(event: PointerEvent): void
{
    const cell = cellAt(event);
    if(cell < 0)
    {
        lastPaintCell = -1;
        return;
    }
    if(cell === lastPaintCell)
    {
        return;
    }
    if(lastPaintCell < 0)
    {
        paintCell(cell, false);
    }
    else
    {
        let x = lastPaintCell % kGridSize;
        let y = Math.floor(lastPaintCell / kGridSize);
        const endX = cell % kGridSize;
        const endY = Math.floor(cell / kGridSize);
        const dx = Math.abs(endX - x);
        const dy = -Math.abs(endY - y);
        const stepX = x < endX ? 1 : -1;
        const stepY = y < endY ? 1 : -1;
        let error = dx + dy;
        while(x !== endX || y !== endY)
        {
            const twiceError = 2 * error;
            if(twiceError >= dy)
            {
                error += dy;
                x += stepX;
            }
            if(twiceError <= dx)
            {
                error += dx;
                y += stepY;
            }
            paintCell(y * kGridSize + x, false);
        }
    }
    lastPaintCell = cell;
}

function startPaint(event: PointerEvent): void
{
    if(paintPointerId !== null || panPointerId !== null)
    {
        return;
    }
    if(event.button === 2)
    {
        const cell = cellAt(event);
        if(cell >= 0)
        {
            editHistory.begin(project, "Erase cell");
            paintCell(cell, true);
            commitEdit();
        }
        return;
    }
    if(event.button !== 0)
    {
        return;
    }
    event.preventDefault();
    editHistory.begin(project, "Paint stroke");
    paintPointerId = event.pointerId;
    lastPaintCell = -1;
    controls.map.setPointerCapture(event.pointerId);
    paintDragAt(event);
}

function movePaint(event: PointerEvent): void
{
    if(event.pointerId !== paintPointerId)
    {
        return;
    }
    if((event.buttons & 1) === 0)
    {
        endPaint(event);
        return;
    }
    paintDragAt(event);
}

function endPaint(event: PointerEvent): void
{
    if(event.pointerId !== paintPointerId)
    {
        return;
    }
    finishStroke();
}

/// Capture panning on the viewport so dragging can continue outside the map.
function startPan(event: PointerEvent): void
{
    if(event.button !== 1 || paintPointerId !== null || panPointerId !== null)
    {
        return;
    }
    event.preventDefault();
    panPointerId = event.pointerId;
    panX = event.clientX;
    panY = event.clientY;
    controls.mapViewport.setPointerCapture(event.pointerId);
    controls.mapViewport.classList.add("panning");
}

function movePan(event: PointerEvent): void
{
    if(event.pointerId !== panPointerId)
    {
        return;
    }
    if((event.buttons & 4) === 0)
    {
        endPan(event);
        return;
    }
    controls.mapViewport.scrollLeft += panX - event.clientX;
    controls.mapViewport.scrollTop += panY - event.clientY;
    panX = event.clientX;
    panY = event.clientY;
}

function endPan(event: PointerEvent): void
{
    if(event.pointerId !== panPointerId)
    {
        return;
    }
    panPointerId = null;
    controls.mapViewport.classList.remove("panning");
    if(controls.mapViewport.hasPointerCapture(event.pointerId))
    {
        controls.mapViewport.releasePointerCapture(event.pointerId);
    }
}

/// Resize only the displayed canvas and retain the map point under the cursor.
function zoomMap(event: WheelEvent): void
{
    if(!event.altKey) { return; }
    event.preventDefault();
    if(paintPointerId !== null || panPointerId !== null)
    {
        return;
    }
    const viewport = controls.mapViewport;
    const unit = event.deltaMode === WheelEvent.DOM_DELTA_LINE ? 16
        : event.deltaMode === WheelEvent.DOM_DELTA_PAGE ? viewport.clientHeight : 1;
    const nextZoom = Math.max(1 / 16, Math.min(4, project.view.zoom * Math.exp(-event.deltaY * unit * 0.002)));
    const bounds = controls.map.getBoundingClientRect();
    const ratio = nextZoom / project.view.zoom;
    const offsetX = event.clientX - bounds.left;
    const offsetY = event.clientY - bounds.top;
    const scrollLeft = viewport.scrollLeft;
    const scrollTop = viewport.scrollTop;
    const size = kGridSize * kCellSize * nextZoom;
    controls.map.style.width = `${size}px`;
    controls.map.style.height = `${size}px`;
    viewport.scrollLeft = scrollLeft + offsetX * (ratio - 1);
    viewport.scrollTop = scrollTop + offsetY * (ratio - 1);
    project.view.zoom = nextZoom;
    session.changed();
    updateGrid();
    saveView();
}

function makeSelect(options: readonly string[], value: number): HTMLSelectElement
{
    const select = document.createElement("select");
    options.forEach((label, index) => {
        const option = document.createElement("option");
        option.value = String(index);
        option.textContent = label;
        select.append(option);
    });
    select.value = String(value);
    return select;
}

function renderPalette(): void
{
    controls.palette.replaceChildren();
    project.layers.forEach((layer) => {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "material-choice";
        button.setAttribute("aria-pressed", String(layer.id === project.activeLayerId));
        const swatch = document.createElement("span");
        swatch.className = "material-preview";
        swatch.setAttribute("aria-hidden", "true");
        if(layer.sheet !== null && sheetFitsMap(layer, project.tileSize))
        {
            swatch.style.backgroundImage = `url(${JSON.stringify(layer.sheet.url)})`;
            swatch.style.backgroundSize = `${layer.atlasGrid * 100}% ${layer.atlasGrid * 100}%`;
        }
        const filename = layer.sheet.label;
        button.title = layer.sheet.id.split("/").slice(-2).join("/");
        button.setAttribute("aria-label", filename);
        button.append(swatch);
        button.addEventListener("click", () => setActiveLayer(layer.id));
        controls.palette.append(button);
    });
}

function setActiveLayer(id: number): void
{
    if(id === project.activeLayerId)
    {
        return;
    }
    project.activeLayerId = id;
    session.changed();
    renderPalette();
    controls.status.value = getLayer(id) === project.layers[0]
        ? "Base selected. Left-click or drag to clear overlays and reveal the background."
        : `${getLayer(id).sheet.label} selected. Left-click or drag to fill and clear higher layers, right-click to clear this layer.`;
    drawCoverageRegion(0, 0, kGridSize, kGridSize);
}

function addSheetInfo(layer: Layer, card: HTMLDivElement): void
{
    const info = document.createElement("span");
    info.className = "hint sheet-info";
    if(layer.sheet === null)
    {
        info.textContent = "No sheet loaded";
    }
    else if(sheetFitsMap(layer, project.tileSize))
    {
        info.textContent = `${layer.sheet.label} · ${layer.sheet.dimension}×${layer.sheet.dimension} px · ${layer.atlasGrid}×${layer.atlasGrid} tiles of ${project.tileSize} px`;
    }
    else
    {
        info.textContent = `${layer.sheet.label} · ${layer.sheet.dimension}×${layer.sheet.dimension} px · incompatible with B = ${project.tileSize}`;
    }
    card.append(info);
}

function updateMapTileSize(): void
{
    const requestedSize = Number(controls.mapTileSize.value);
    const valid = Number.isInteger(requestedSize) && requestedSize > 0
        && project.layers.every((layer) => sheetFitsMap(layer, requestedSize));
    if(!valid)
    {
        controls.mapTileSize.value = String(project.tileSize);
        controls.status.value = "Choose B so every layer's square sheet divides into an even A×A grid.";
        return;
    }
    edit("Change tile size", () => {
        project.tileSize = requestedSize;
        for(const layer of project.layers) { setLayerTileSize(layer, requestedSize); }
    });
}

function renderLayers(): void
{
    controls.layers.replaceChildren();
    project.layers.forEach((layer, index) => {
        const card = document.createElement("div");
        card.className = "layer-card";
        const heading = document.createElement("strong");
        heading.textContent = index === 0 ? "Base layer" : `Layer ${index}`;
        const materialLabel = document.createElement("label");
        materialLabel.textContent = "Material";
        const choices = availableTiles();
        const materialSelect = makeSelect(choices.map((atlas) => atlas.label), choices.indexOf(layer.sheet));
        materialSelect.title = "Choose the tile sheet for this layer.";
        materialSelect.addEventListener("change", () => edit("Change material", () => {
            const sheet = choices[Number(materialSelect.value)];
            if(atlasGrid(sheet.dimension, project.tileSize) === 0)
            {
                throw new Error(`${sheet.label} is incompatible with tile size ${project.tileSize}`);
            }
            layer.sheet = sheet;
            setLayerTileSize(layer, project.tileSize);
        }));
        materialLabel.append(materialSelect);
        card.append(heading, materialLabel);
        addSheetInfo(layer, card);

        if(index !== 0)
        {
            const actions = document.createElement("div");
            actions.className = "layer-actions";
            const up = document.createElement("button");
            up.type = "button";
            up.textContent = "Move up";
            up.title = "Move this layer earlier in the list, beneath the preceding overlay.";
            up.disabled = index === 1;
            up.addEventListener("click", () => edit("Move layer up", () => {
                const current = project.layers.indexOf(layer);
                [project.layers[current - 1], project.layers[current]] =
                    [project.layers[current], project.layers[current - 1]];
            }));
            const down = document.createElement("button");
            down.type = "button";
            down.textContent = "Move down";
            down.title = "Move this layer later in the list, above the following overlay.";
            down.disabled = index === project.layers.length - 1;
            down.addEventListener("click", () => edit("Move layer down", () => {
                const current = project.layers.indexOf(layer);
                [project.layers[current], project.layers[current + 1]] =
                    [project.layers[current + 1], project.layers[current]];
            }));
            const remove = document.createElement("button");
            remove.type = "button";
            remove.textContent = "Remove";
            remove.title = "Remove this layer and its painted cells. This edit can be undone.";
            remove.addEventListener("click", () => edit("Remove layer", () => {
                project.layers.splice(project.layers.indexOf(layer), 1);
                if(project.activeLayerId === layer.id)
                {
                    project.activeLayerId = project.layers[Math.max(0, index - 1)].id;
                }
            }));
            actions.append(up, down, remove);
            card.append(actions);
        }
        controls.layers.append(card);
    });
    renderPalette();
}

function addOverlay(): void
{
    edit("Add overlay", () => {
        if(project.layers.length >= 256) { throw new Error("Projects support at most 256 layers"); }
        const choices = availableTiles().filter((asset) => atlasGrid(asset.dimension, project.tileSize) > 0);
        const sheet = choices[Math.min(2, choices.length - 1)];
        const layer: Layer = {
            id: project.nextLayerId++, painted: Array(4096).fill(false),
            sheet, atlasGrid: atlasGrid(sheet.dimension, project.tileSize), tileSize: project.tileSize
        };
        project.layers.push(layer);
        project.activeLayerId = layer.id;
    });
}

function resetSample(): void
{
    edit("Load sample", () => {
        const choices = availableTiles().filter((asset) => atlasGrid(asset.dimension, project.tileSize) > 0);
        const makeLayer = (index: number): Layer => {
            const sheet = choices[Math.min(index, choices.length - 1)];
            const id = project.nextLayerId++;
            return { id, sheet, tileSize: project.tileSize,
                atlasGrid: atlasGrid(sheet.dimension, project.tileSize),
                painted: Array(4096).fill(index === 0) };
        };
        project.layers = [makeLayer(0), makeLayer(1), makeLayer(2)];
        const full = project.layers[1];
        const patch = project.layers[2];
        for(let y = 0; y < kGridSize; y++)
        {
            for(let x = 0; x < kGridSize; x++) { full.painted[y * kGridSize + x] = true; }
        }
        for(let y = 1; y <= 3; y++)
        {
            for(let x = 1; x <= 3; x++) { patch.painted[y * kGridSize + x] = true; }
        }
        patch.painted[3 * kGridSize + 2] = false;
        patch.painted[kGridSize + 1] = false;
        project.activeLayerId = patch.id;
    });
}

controls.mapViewport.addEventListener("pointerdown", startPan);
controls.mapViewport.addEventListener("pointermove", movePan);
controls.mapViewport.addEventListener("pointerup", endPan);
controls.mapViewport.addEventListener("pointercancel", endPan);
controls.mapViewport.addEventListener("lostpointercapture", endPan);
controls.mapViewport.addEventListener("auxclick", (event) => {
    if(event.button === 1)
    {
        event.preventDefault();
    }
});
controls.mapViewport.addEventListener("wheel", zoomMap, { passive: false });
controls.map.addEventListener("pointerdown", startPaint);
controls.map.addEventListener("pointermove", movePaint);
controls.map.addEventListener("pointerup", endPaint);
controls.map.addEventListener("pointercancel", endPaint);
controls.map.addEventListener("lostpointercapture", endPaint);
controls.map.addEventListener("contextmenu", (event) => event.preventDefault());
controls.addLayer.addEventListener("click", addOverlay);
controls.activeMask.addEventListener("change", () => edit("Change mask mode", () => {
    project.maskMode = controls.activeMask.value === "random" ? "random" : Number(controls.activeMask.value);
}));
controls.showGrid.addEventListener("change", () => { updateGrid(); saveView(); });
controls.mapViewport.addEventListener("scroll", saveView);
controls.mapTileSize.addEventListener("change", updateMapTileSize);
required("reset", HTMLButtonElement).addEventListener("click", resetSample);

function renderMaskChoices(): void
{
    controls.activeMask.replaceChildren(new Option("Random per junction", "random"));
    const links = required("mask-links", HTMLParagraphElement);
    links.replaceChildren();
    project.masks.forEach((mask, index) => {
        controls.activeMask.add(new Option(mask.label, String(index)));
        const link = document.createElement("a");
        link.href = mask.url;
        link.textContent = mask.label;
        if(index > 0) { links.append(" · "); }
        links.append(link);
    });
    controls.activeMask.value = String(project.maskMode);
}

async function start(): Promise<void>
{
    try
    {
        await refreshProjects();
        if(required("project-choice", HTMLSelectElement).value) { await fileAction("open"); }
    }
    catch(error) { showError(error); }
    finally { required("mosaic", HTMLElement).inert = false; updateProjectButtons(); }
}

function showError(error: unknown): void
{
    controls.status.value = error instanceof Error ? error.message : String(error);
}

function updateSaveStatus(status: SaveStatus): void
{
    const state = status.error ? `Save failed: ${status.error} — press Ctrl/Cmd+S to retry`
        : status.saving ? "Saving…" : status.dirty ? "Unsaved" : "Saved";
    required("save-status", HTMLOutputElement).value = `${status.filename ? decodeURIComponent(new URL(status.filename).pathname.split("/").at(-2)!) : "No project"} — ${state}`;
}

const session = new ProjectSession(() => editHistory.committedState(project), writeProject, updateSaveStatus, 600);

function availableTiles(): TileAtlas[]
{
    return [...new Set([...tileAtlases, ...(ready ? project.layers.map((layer) => layer.sheet) : [])])];
}

function collectAssets(): void
{
    const retained = new Set([...tileAtlases, ...projectMasks,
        ...(ready ? documentAssets(project) : []), ...editHistory.retainedStates().flatMap(documentAssets)]);
    for(const asset of assets)
    {
        if(!retained.has(asset))
        {
            URL.revokeObjectURL(asset.url);
            assets.delete(asset);
        }
    }
}

function refreshDocument(): void
{
    controls.mapTileSize.value = String(project.tileSize);
    controls.showGrid.checked = project.view.showGrid;
    const size = kGridSize * kCellSize * project.view.zoom;
    controls.map.style.width = `${size}px`;
    controls.map.style.height = `${size}px`;
    renderMaskChoices();
    renderLayers();
    draw();
    updateGrid();
    controls.mapViewport.scrollLeft = project.view.scrollLeft;
    controls.mapViewport.scrollTop = project.view.scrollTop;
    updateProjectButtons();
}

function installDocument(next: ProjectDocument): void
{
    validateDocument(next);
    project = next;
    ready = true;
    editHistory.clear();
    for(const asset of documentAssets(next)) { assets.add(asset); }
    required("editor", HTMLDivElement).inert = false;
    initializeGrid();
    refreshDocument();
    collectAssets();
    controls.status.value = "";
}

function saveView(): void
{
    if(!ready || fileBusy) { return; }
    const left = controls.mapViewport.scrollLeft;
    const top = controls.mapViewport.scrollTop;
    const grid = controls.showGrid.checked;
    if(project.view.scrollLeft !== left || project.view.scrollTop !== top || project.view.showGrid !== grid)
    {
        project.view.scrollLeft = left;
        project.view.scrollTop = top;
        project.view.showGrid = grid;
        session.changed();
    }
}

function commitEdit(): void
{
    validateDocument(project);
    if(editHistory.commit(project)) { session.changed(); }
    updateProjectButtons();
    collectAssets();
}

function finishStroke(): void
{
    if(paintPointerId === null) { return; }
    const pointer = paintPointerId;
    paintPointerId = null;
    lastPaintCell = -1;
    if(controls.map.hasPointerCapture(pointer)) { controls.map.releasePointerCapture(pointer); }
    commitEdit();
}

function edit(label: string, mutate: () => void): void
{
    finishStroke();
    editHistory.begin(project, label);
    try
    {
        mutate();
        commitEdit();
    }
    catch(error)
    {
        const before = editHistory.rollback();
        if(before !== null) { project = before; }
        showError(error);
    }
    refreshDocument();
    collectAssets();
}

function changeHistory(redo: boolean): void
{
    if(!ready || fileBusy) { return; }
    finishStroke();
    const restored = redo ? editHistory.redo(project) : editHistory.undo(project);
    if(restored === null) { return; }
    restored.view = { ...project.view };
    if(restored.layers.some((layer) => layer.id === project.activeLayerId))
    {
        restored.activeLayerId = project.activeLayerId;
    }
    project = restored;
    session.changed();
    refreshDocument();
    collectAssets();
}

let currentFolder: string | null = null;
let projectListing = false;
let projectListingRequest = 0;

function updateProjectButtons(): void
{
    required("project-open", HTMLButtonElement).disabled = fileBusy;
    updateChooserButtons();
    required("project-reload", HTMLButtonElement).disabled = !ready;
    const undo = required("project-undo", HTMLButtonElement);
    const redo = required("project-redo", HTMLButtonElement);
    undo.disabled = !ready || editHistory.undoLabel === null;
    redo.disabled = !ready || editHistory.redoLabel === null;
    undo.textContent = editHistory.undoLabel === null ? "Undo" : `Undo ${editHistory.undoLabel}`;
    redo.textContent = editHistory.redoLabel === null ? "Redo" : `Redo ${editHistory.redoLabel}`;
}

function updateChooserButtons(): void
{
    const busy = projectListing || fileBusy;
    const choices = required("project-choice", HTMLSelectElement);
    choices.disabled = busy;
    required("project-confirm", HTMLButtonElement).disabled = busy || !choices.value;
    required("project-refresh", HTMLButtonElement).disabled = busy;
    required("project-cancel", HTMLButtonElement).disabled = fileBusy;
}

async function refreshProjects(): Promise<void>
{
    const request = ++projectListingRequest;
    const choices = required("project-choice", HTMLSelectElement);
    const dialog = required("project-dialog", HTMLDialogElement);
    const message = required("project-message", HTMLOutputElement);
    const selected = choices.value;
    projectListing = true;
    message.value = "Loading projects…";
    updateChooserButtons();
    try
    {
        const urls = await listDirectory(new URL("/projects/", location.href).href, true);
        if(request !== projectListingRequest) { return; }
        choices.replaceChildren(...urls.map((url) =>
            new Option(decodeURIComponent(new URL(url).pathname.split("/").at(-2)!), url)));
        choices.value = urls.includes(selected) ? selected : urls[0] ?? "";
        message.value = urls.length ? "Select a project, then click Open."
            : "No projects found. Add a project folder, then click Refresh Projects.";
        if(!dialog.open && !urls.length) { controls.status.value = message.value; }
    }
    catch(error)
    {
        if(request !== projectListingRequest) { return; }
        choices.replaceChildren();
        message.value = error instanceof Error ? error.message : String(error);
        if(!dialog.open) { showError(error); }
    }
    finally
    {
        if(request === projectListingRequest)
        {
            projectListing = false;
            updateProjectButtons();
        }
    }
}

async function showProjectChooser(): Promise<void>
{
    if(fileBusy) { return; }
    finishStroke();
    const dialog = required("project-dialog", HTMLDialogElement);
    if(dialog.open) { return; }
    if(currentFolder !== null) { required("project-choice", HTMLSelectElement).value = currentFolder; }
    dialog.showModal();
    await refreshProjects();
}

async function fileAction(action: "open" | "save" | "reload"): Promise<void>
{
    if(fileBusy || (action === "open" && projectListing)) { return; }
    finishStroke();
    fileBusy = true;
    updateProjectButtons();
    required("mosaic", HTMLElement).inert = true;
    try
    {
        if(action === "save") { await session.flush(); return; }
        const folder = action === "reload" ? currentFolder : required("project-choice", HTMLSelectElement).value;
        if(!folder) { return; }
        if(action === "open") { required("project-message", HTMLOutputElement).value = "Opening project…"; }
        if(ready) { await session.flush(); }
        const next = await loadProject(folder);
        tileAtlases = next.tiles;
        projectMasks = next.masks;
        for(const asset of [...next.tiles, ...next.masks]) { assets.add(asset); }
        installDocument(next.project);
        currentFolder = folder;
        session.attach(new URL("level.json", folder).href, next.saved);
        if(action === "open") { required("project-dialog", HTMLDialogElement).close(); }
    }
    catch(error)
    {
        showError(error);
        if(action === "open")
        {
            required("project-message", HTMLOutputElement).value = error instanceof Error ? error.message : String(error);
        }
    }
    finally
    {
        fileBusy = false;
        required("mosaic", HTMLElement).inert = false;
        updateProjectButtons();
        collectAssets();
    }
}

required("project-reload", HTMLButtonElement).addEventListener("click", () => { void fileAction("reload"); });
required("project-open", HTMLButtonElement).addEventListener("click", () => { void showProjectChooser(); });
required("project-confirm", HTMLButtonElement).addEventListener("click", () => { void fileAction("open"); });
required("project-refresh", HTMLButtonElement).addEventListener("click", () => { void refreshProjects(); });
required("project-cancel", HTMLButtonElement).addEventListener("click", () => {
    required("project-dialog", HTMLDialogElement).close();
});
required("project-dialog", HTMLDialogElement).addEventListener("cancel", (event) => {
    if(fileBusy) { event.preventDefault(); }
});
required("project-dialog", HTMLDialogElement).addEventListener("close", () => {
    // Ignore a directory response that arrives after the user cancels.
    projectListingRequest++;
    projectListing = false;
    updateChooserButtons();
});
required("project-choice", HTMLSelectElement).addEventListener("change", updateChooserButtons);
required("project-choice", HTMLSelectElement).addEventListener("keydown", (event) => {
    if(event.key === "Enter")
    {
        event.preventDefault();
        if(!required("project-confirm", HTMLButtonElement).disabled) { void fileAction("open"); }
    }
});
required("project-undo", HTMLButtonElement).addEventListener("click", () => changeHistory(false));
required("project-redo", HTMLButtonElement).addEventListener("click", () => changeHistory(true));
globalThis.addEventListener("beforeunload", (event) => {
    if(fileBusy || (ready && (session.dirty || editHistory.inTransaction)))
    {
        event.preventDefault();
        event.returnValue = "";
    }
});
globalThis.addEventListener("keydown", (event) => {
    if(!(event.ctrlKey || event.metaKey) || event.altKey || fileBusy
        || required("project-dialog", HTMLDialogElement).open) { return; }
    const target = event.target;
    if(target instanceof HTMLElement && (target.isContentEditable || target.closest("input, textarea, select"))) { return; }
    const key = event.key.toLowerCase();
    if(key === "z" || key === "y")
    {
        event.preventDefault();
        changeHistory(key === "y" || event.shiftKey);
    }
    else if(key === "s" && ready)
    {
        event.preventDefault();
        void fileAction("save");
    }
});

void start();

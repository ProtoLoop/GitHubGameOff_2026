import { listDirectory, loadProject } from "./project-http.ts";
import { documentAssets, sameTerrainContent,
    validateDocument, type ProjectDocument, type Layer, type ImageAsset as TileAtlas, type PropAsset } from "./document.ts";
import { History } from "./history.ts";
import { writeProject } from "./project-file.ts";
import { drawTerrainRegion, drawCoverageRegion, drawProps, type ImagePreview } from "./renderer.ts";
import { kGridSize, kCellSize, kCellCount } from "./grid.ts";
import { ProjectSession, type SaveStatus } from "./project-session.ts";
import { mapPoint, clampPropPoint, hitProp, propsInBox, groupMoveDelta, type Point, type SelectionBox } from "./props.ts";

let project: ProjectDocument;
let ready = false;
let fileBusy = false;
const assets = new Set<TileAtlas | PropAsset>();
const editHistory = new History(100, 64 * 1024 * 1024);
let paintPointerId: number | null = null;
let lastPaintCell = -1;
let panPointerId: number | null = null;
let panX = 0;
let panY = 0;
let tool: "terrain" | "place" | "select" = "terrain";
let selectedPropIds = new Set<number>();
let selectedPropAsset: PropAsset | null = null;
let propPointerId: number | null = null;
let hoverClient: Point | null = null;
let propGesture: {
    readonly kind: "move" | "box";
    readonly start: Point;
    readonly clientStart: Point;
    readonly beforeSelection: Set<number>;
    readonly baseSelection: Set<number>;
    readonly positions: { id: number; x: number; y: number }[];
    readonly clickedId: number | null;
    end: Point;
    moved: boolean;
} | null = null;

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
    showGrid : required("show-grid", HTMLInputElement)
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


function drawPropOverlay(): void
{
    let preview: ImagePreview | null = null;
    const asset = tool === "place" ? selectedPropAsset : tool === "terrain" ? getLayer(project.activeLayerId).sheet : null;
    if(asset && hoverClient && panPointerId === null && !fileBusy
        && !required("project-dialog", HTMLDialogElement).open)
    {
        const bounds = controls.map.getBoundingClientRect();
        const point = mapPoint(hoverClient.x, hoverClient.y, bounds);
        const size = kGridSize * kCellSize;
        if(point.x >= 0 && point.y >= 0 && point.x <= size && point.y <= size)
        {
            if(tool === "terrain")
            {
                const thumbnail = controls.palette.querySelector<HTMLElement>('[aria-pressed="true"] .material-preview');
                if(thumbnail)
                {
                    const rect = thumbnail.getBoundingClientRect();
                    // A collapsed palette has no layout box, but retains its responsive CSS dimensions.
                    const style = getComputedStyle(thumbnail);
                    const width = rect.width || Number.parseFloat(style.width);
                    const height = rect.height || Number.parseFloat(style.height);
                    preview = { asset, ...point, size: {
                        width: width * size / bounds.width,
                        height: height * size / bounds.height
                    } };
                }
            }
            else { preview = { asset, ...point }; }
        }
    }
    const box: SelectionBox | null = propGesture?.kind === "box" && propGesture.moved
        ? { start: propGesture.start, end: propGesture.end } : null;
    drawProps(project, required("prop-overlay", HTMLCanvasElement), selectedPropIds, preview, box);
}

function updatePropControls(): void
{
    selectedPropIds = new Set(project.props.filter((prop) => selectedPropIds.has(prop.id)).map((prop) => prop.id));
    const selected = selectedPropIds.size === 1 ? project.props.find((prop) => selectedPropIds.has(prop.id)) : undefined;
    required("prop-selection", HTMLOutputElement).value = selected?.asset.label
        ?? (selectedPropIds.size > 1 ? `${selectedPropIds.size} props selected` : "No prop selected");
}

function renderPropPalette(): void
{
    if(selectedPropAsset === null || !project.propAssets.includes(selectedPropAsset)) { selectedPropAsset = project.propAssets[0] ?? null; }
    const palette = required("prop-palette", HTMLDivElement);
    palette.replaceChildren();
    for(const asset of project.propAssets)
    {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "material-choice";
        button.title = `${asset.id} (${asset.width}×${asset.height})`;
        button.setAttribute("aria-label", asset.label);
        button.setAttribute("aria-pressed", String(tool === "place" && asset === selectedPropAsset));
        const preview = document.createElement("img");
        preview.src = asset.url;
        preview.alt = "";
        preview.className = "prop-preview";
        button.append(preview);
        button.addEventListener("click", () => {
            selectedPropAsset = asset;
            setTool("place");
        });
        palette.append(button);
    }
    if(project.propAssets.length === 0)
    {
        palette.textContent = "Add images to assets/props/ and reload the project.";
    }
}

function setTool(next: typeof tool): void
{
    finishStroke();
    tool = next;
    if(next !== "select") { selectedPropIds.clear(); }
    controls.map.dataset.tool = tool;
    renderPalette();
    renderPropPalette();
    updatePropControls();
    drawPropOverlay();
}

function propPoint(event: PointerEvent): { x: number; y: number }
{
    return mapPoint(event.clientX, event.clientY, controls.map.getBoundingClientRect());
}

function startProp(event: PointerEvent): void
{
    if(event.button === 2 && tool === "place")
    {
        event.preventDefault();
        controls.map.focus({ preventScroll: true });
        setTool("select");
        return;
    }
    if(event.button !== 0 || paintPointerId !== null || panPointerId !== null || propPointerId !== null) { return; }
    event.preventDefault();
    controls.map.focus({ preventScroll: true });
    const point = propPoint(event);
    if(tool === "place")
    {
        const asset = selectedPropAsset;
        if(!asset) { setMessage(controls.status, "Add an image to assets/props/ and reload the project.", true); return; }
        if(project.props.length >= 10000) { setMessage(controls.status, "The level already contains 10,000 props.", true); return; }
        edit("Place prop", () => {
            const id = project.nextPropId++;
            project.props.push({ id, asset, ...clampPropPoint(point.x, point.y), rotation: 0 });
            selectedPropIds = new Set([id]);
        });
        return;
    }
    const prop = hitProp(project.props, point.x, point.y);
    const beforeSelection = new Set(selectedPropIds);
    if(prop && event.shiftKey)
    {
        if(!selectedPropIds.delete(prop.id)) { selectedPropIds.add(prop.id); }
        updatePropControls();
        drawPropOverlay();
        return;
    }
    if(prop && !selectedPropIds.has(prop.id)) { selectedPropIds = new Set([prop.id]); }
    if(!prop && !event.shiftKey) { selectedPropIds.clear(); }
    propGesture = {
        kind: prop ? "move" : "box", start: point, end: point,
        clientStart: { x: event.clientX, y: event.clientY }, beforeSelection,
        baseSelection: new Set(selectedPropIds), clickedId: prop?.id ?? null, moved: false,
        positions: prop ? project.props.filter((candidate) => selectedPropIds.has(candidate.id))
            .map(({ id, x, y }) => ({ id, x, y })) : []
    };
    if(prop) { editHistory.begin(project, selectedPropIds.size > 1 ? "Move props" : "Move prop"); }
    propPointerId = event.pointerId;
    controls.map.setPointerCapture(event.pointerId);
    updatePropControls();
    drawPropOverlay();
}

function updatePropGesture(event: PointerEvent): void
{
    const gesture = propGesture;
    if(event.pointerId !== propPointerId || !gesture) { return; }
    const point = propPoint(event);
    gesture.end = clampPropPoint(point.x, point.y);
    gesture.moved ||= Math.hypot(event.clientX - gesture.clientStart.x, event.clientY - gesture.clientStart.y) >= 3;
    if(!gesture.moved) { return; }
    if(gesture.kind === "box")
    {
        selectedPropIds = new Set([...gesture.baseSelection, ...propsInBox(project.props, gesture)]);
        updatePropControls();
    }
    else
    {
        const delta = groupMoveDelta(gesture.positions, point.x - gesture.start.x, point.y - gesture.start.y);
        const originals = new Map(gesture.positions.map((position) => [position.id, position]));
        for(const prop of project.props)
        {
            const original = originals.get(prop.id);
            if(original) { prop.x = original.x + delta.x; prop.y = original.y + delta.y; }
        }
    }
    drawPropOverlay();
}

function moveProp(event: PointerEvent): void
{
    if(event.pointerId !== propPointerId) { return; }
    if((event.buttons & 1) === 0) { finishPropDrag(false); return; }
    updatePropGesture(event);
}

function finishPropDrag(cancel: boolean): void
{
    if(propPointerId === null || !propGesture) { return; }
    const pointer = propPointerId;
    const gesture = propGesture;
    propPointerId = null;
    propGesture = null;
    if(controls.map.hasPointerCapture(pointer)) { controls.map.releasePointerCapture(pointer); }
    if(cancel)
    {
        if(gesture.kind === "move")
        {
            const before = editHistory.rollback();
            if(before) { before.view = { ...project.view }; project = before; }
        }
        selectedPropIds = gesture.beforeSelection;
        updateProjectButtons();
    }
    else if(gesture.kind === "move")
    {
        if(!gesture.moved && gesture.clickedId !== null) { selectedPropIds = new Set([gesture.clickedId]); }
        commitEdit();
    }
    updatePropControls();
    drawPropOverlay();
}

function deleteSelectedProp(): void
{
    if(selectedPropIds.size === 0) { return; }
    edit(selectedPropIds.size > 1 ? "Delete props" : "Delete prop", () => {
        project.props = project.props.filter((prop) => !selectedPropIds.has(prop.id));
        selectedPropIds.clear();
    });
}

function drawRegion(startX: number, startY: number, endX: number, endY: number): void
{
    drawTerrainRegion(project, controls.map, startX, startY, endX, endY);
    drawCoverageRegion(project, controls.coverage, startX, startY, endX, endY);
}

function draw(): void
{
    drawRegion(0, 0, kGridSize, kGridSize);
    drawPropOverlay();
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
        setMessage(controls.status, "The base cannot be cleared. Left-click to reveal it.", false);
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
    setMessage(controls.status, erase
        ? `${layer.sheet.label}: cleared ${tileX + 1}, ${tileY + 1}`
        : `${layer.sheet.label}: ${layer === project.layers[0] ? "revealed" : "occupied"} ${tileX + 1}, ${tileY + 1}; higher layers cleared`, false);
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
    if(tool !== "terrain") { startProp(event); return; }
    if(paintPointerId !== null || panPointerId !== null || propPointerId !== null)
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
    controls.map.focus({ preventScroll: true });
    editHistory.begin(project, "Paint stroke");
    paintPointerId = event.pointerId;
    lastPaintCell = -1;
    controls.map.setPointerCapture(event.pointerId);
    paintDragAt(event);
}

function movePaint(event: PointerEvent): void
{
    if(propPointerId !== null) { moveProp(event); return; }
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
    if(event.pointerId === propPointerId)
    {
        if(event.type === "pointerup") { updatePropGesture(event); }
        finishPropDrag(event.type === "pointercancel");
        return;
    }
    if(event.pointerId !== paintPointerId)
    {
        return;
    }
    finishStroke();
}

/// Capture panning on the viewport so dragging can continue outside the map.
function startPan(event: PointerEvent): void
{
    if(event.button !== 1 || paintPointerId !== null || panPointerId !== null || propPointerId !== null)
    {
        return;
    }
    event.preventDefault();
    panPointerId = event.pointerId;
    panX = event.clientX;
    panY = event.clientY;
    controls.mapViewport.setPointerCapture(event.pointerId);
    controls.mapViewport.classList.add("panning");
    drawPropOverlay();
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
    drawPropOverlay();
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
    if(paintPointerId !== null || panPointerId !== null || propPointerId !== null)
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
    drawPropOverlay();
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
        button.setAttribute("aria-pressed", String(tool === "terrain" && layer.id === project.activeLayerId));
        const swatch = document.createElement("span");
        swatch.className = "material-preview";
        swatch.setAttribute("aria-hidden", "true");
        swatch.style.backgroundImage = `url(${JSON.stringify(layer.sheet.url)})`;
        swatch.style.backgroundSize = "100% 100%";
        const filename = layer.sheet.label;
        button.title = layer.sheet.id;
        button.setAttribute("aria-label", filename);
        button.append(swatch);
        button.addEventListener("click", () => setActiveLayer(layer.id));
        controls.palette.append(button);
    });
}

function setActiveLayer(id: number): void
{
    setTool("terrain");
    if(id === project.activeLayerId)
    {
        return;
    }
    project.activeLayerId = id;
    session.changed();
    renderPalette();
    setMessage(controls.status, getLayer(id) === project.layers[0]
        ? "Base selected. Left-click or drag to clear overlays and reveal the background."
        : `${getLayer(id).sheet.label} selected. Left-click or drag to fill and clear higher layers, right-click to clear this layer.`, false);
    drawCoverageRegion(project, controls.coverage, 0, 0, kGridSize, kGridSize);
    drawPropOverlay();
}

function addSheetInfo(layer: Layer, card: HTMLDivElement): void
{
    const info = document.createElement("span");
    info.className = "hint sheet-info";
    const cells = layer.sheet.dimension / kCellSize;
    info.textContent = `${layer.sheet.label} · ${layer.sheet.dimension}×${layer.sheet.dimension} px texture · repeats every ${cells}×${cells} cells`;
    card.append(info);
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
            layer.sheet = sheet;
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
        const choices = availableTiles();
        const sheet = choices[Math.min(2, choices.length - 1)];
        const layer: Layer = {
            id: project.nextLayerId++, painted: Array(kCellCount).fill(false),
            sheet
        };
        project.layers.push(layer);
        project.activeLayerId = layer.id;
    });
}

function resetSample(): void
{
    edit("Load sample", () => {
        const choices = availableTiles();
        const makeLayer = (index: number): Layer => {
            const sheet = choices[Math.min(index, choices.length - 1)];
            const id = project.nextLayerId++;
            return { id, sheet,
                painted: Array(kCellCount).fill(index === 0) };
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
controls.mapViewport.addEventListener("scroll", () => {
    saveView();
    if(ready) { drawPropOverlay(); }
});
controls.mapViewport.addEventListener("pointermove", (event) => {
    hoverClient = event.pointerType === "touch" ? null : { x: event.clientX, y: event.clientY };
    if(ready && tool !== "select") { drawPropOverlay(); }
});
controls.mapViewport.addEventListener("pointerleave", () => {
    hoverClient = null;
    if(ready) { drawPropOverlay(); }
});
globalThis.addEventListener("resize", () => { if(ready) { drawPropOverlay(); } });
globalThis.addEventListener("blur", () => {
    hoverClient = null;
    if(ready) { finishPropDrag(true); drawPropOverlay(); }
});
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

function setMessage(output: HTMLOutputElement, message: string, error: boolean): void
{
    output.value = message;
    output.classList.toggle("error", error);
}

function showError(error: unknown): void
{
    setMessage(controls.status, error instanceof Error ? error.message : String(error), true);
}

function updateSaveStatus(status: SaveStatus): void
{
    const state = status.error ? `Save failed: ${status.error} — press Ctrl/Cmd+S to retry`
        : status.saving ? "Saving…" : status.dirty ? "Unsaved" : "Saved";
    setMessage(required("save-status", HTMLOutputElement), `${status.filename ? decodeURIComponent(new URL(status.filename).pathname.split("/").at(-2)!) : "No project"} — ${state}`, status.error !== null);
}

const session = new ProjectSession(() => editHistory.committedState(project), writeProject, updateSaveStatus, 600);

function availableTiles(): TileAtlas[]
{
    // Preserve directory-listing order in material controls without storing another catalog.
    return project.tileSheets.toSorted((a, b) => {
        const aPath = a.id.split("/").map(encodeURIComponent).join("/");
        const bPath = b.id.split("/").map(encodeURIComponent).join("/");
        return aPath < bPath ? -1 : aPath > bPath ? 1 : 0;
    });
}

function collectAssets(): void
{
    const retained = new Set([...(ready ? documentAssets(project) : []), ...editHistory.retainedStates().flatMap(documentAssets)]);
    for(const asset of assets)
    {
        if(!retained.has(asset))
        {
            URL.revokeObjectURL(asset.url);
            assets.delete(asset);
        }
    }
}

/// Refresh controls against the current snapshot, but reuse unchanged terrain pixels.
/// Pass null when installing a project. Otherwise previous describes the rendered terrain.
function refreshDocument(previous: ProjectDocument | null): void
{
    controls.showGrid.checked = project.view.showGrid;
    const size = kGridSize * kCellSize * project.view.zoom;
    controls.map.style.width = `${size}px`;
    controls.map.style.height = `${size}px`;
    renderMaskChoices();
    renderLayers();
    renderPropPalette();
    controls.map.dataset.tool = tool;
    updatePropControls();
    if(previous === null || !sameTerrainContent(previous, project))
    {
        draw();
    }
    else
    {
        const previousLayer = previous.layers.findIndex((layer) => layer.id === previous.activeLayerId);
        const activeLayer = project.layers.findIndex((layer) => layer.id === project.activeLayerId);
        if(previousLayer !== activeLayer)
        {
            drawCoverageRegion(project, controls.coverage, 0, 0, kGridSize, kGridSize);
        }
        drawPropOverlay();
    }
    updateGrid();
    controls.mapViewport.scrollLeft = project.view.scrollLeft;
    controls.mapViewport.scrollTop = project.view.scrollTop;
    updateProjectButtons();
}

function installDocument(next: ProjectDocument): void
{
    validateDocument(next);
    project = next;
    selectedPropIds.clear();
    hoverClient = null;
    selectedPropAsset = null;
    ready = true;
    editHistory.clear();
    for(const asset of documentAssets(next)) { assets.add(asset); }
    required("editor", HTMLDivElement).inert = false;
    initializeGrid();
    refreshDocument(null);
    collectAssets();
    setMessage(controls.status, "", false);
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
    finishPropDrag(false);
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
    const previous = editHistory.committedState(project);
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
    refreshDocument(previous);
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
    const previous = project;
    project = restored;
    session.changed();
    refreshDocument(previous);
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
    setMessage(message, "Loading projects…", false);
    updateChooserButtons();
    try
    {
        const urls = await listDirectory(new URL("/projects/", location.href).href, true);
        if(request !== projectListingRequest) { return; }
        choices.replaceChildren(...urls.map((url) =>
            new Option(decodeURIComponent(new URL(url).pathname.split("/").at(-2)!), url)));
        choices.value = urls.includes(selected) ? selected : urls[0] ?? "";
        setMessage(message, urls.length ? "Select a project, then click Open."
            : "No projects found. Add a project folder, then click Refresh Projects.", false);
        if(!dialog.open && !urls.length) { setMessage(controls.status, message.value, false); }
    }
    catch(error)
    {
        if(request !== projectListingRequest) { return; }
        choices.replaceChildren();
        setMessage(message, error instanceof Error ? error.message : String(error), true);
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
    hoverClient = null;
    if(ready) { drawPropOverlay(); }
    await refreshProjects();
}

async function fileAction(action: "open" | "save" | "reload"): Promise<void>
{
    if(fileBusy || (action === "open" && projectListing)) { return; }
    finishStroke();
    fileBusy = true;
    hoverClient = null;
    if(ready) { drawPropOverlay(); }
    updateProjectButtons();
    required("mosaic", HTMLElement).inert = true;
    try
    {
        if(action === "save") { await session.flush(); return; }
        const folder = action === "reload" ? currentFolder : required("project-choice", HTMLSelectElement).value;
        if(!folder) { return; }
        if(action === "open") { setMessage(required("project-message", HTMLOutputElement), "Opening project…", false); }
        if(ready) { await session.flush(); }
        const next = await loadProject(folder);
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
            setMessage(required("project-message", HTMLOutputElement), error instanceof Error ? error.message : String(error), true);
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

globalThis.addEventListener("keydown", (event) => {
    if(!ready || fileBusy || required("project-dialog", HTMLDialogElement).open) { return; }
    if(event.key === "Escape")
    {
        event.preventDefault();
        const wasSelecting = tool === "select" && propPointerId === null;
        finishPropDrag(true);
        if(paintPointerId !== null)
        {
            const pointer = paintPointerId;
            paintPointerId = null;
            lastPaintCell = -1;
            if(controls.map.hasPointerCapture(pointer)) { controls.map.releasePointerCapture(pointer); }
            const previous = project;
            const before = editHistory.rollback();
            if(before) { before.view = { ...project.view }; project = before; }
            refreshDocument(previous);
        }
        if(panPointerId !== null)
        {
            const pointer = panPointerId;
            panPointerId = null;
            controls.mapViewport.classList.remove("panning");
            if(controls.mapViewport.hasPointerCapture(pointer)) { controls.mapViewport.releasePointerCapture(pointer); }
        }
        if(wasSelecting) { selectedPropIds.clear(); }
        setTool("select");
        return;
    }
    const target = event.target;
    if(target instanceof HTMLElement && (target.isContentEditable || target.closest("input, textarea, select"))) { return; }
    if((event.key === "Delete" || event.key === "Backspace") && selectedPropIds.size > 0
        && !event.ctrlKey && !event.metaKey && !event.altKey)
    {
        event.preventDefault();
        deleteSelectedProp();
    }
});

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

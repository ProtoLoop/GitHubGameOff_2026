import { listDirectory, loadProject } from "./project-http.ts";
import { documentAssets, sameTerrainContent,
    type ProjectDocument, type Layer, type ImageAsset as TileAtlas, type SpriteSequence, type ObjectDefinition } from "./document.ts";
import { Editor, type Tool, type EditResult } from "./editor.ts";
import { History } from "./history.ts";
import { writeProject } from "./project-file.ts";
import { drawTerrainRegion, drawCoverageRegion, drawObjects, propFrameSource, type ImagePreview, type RenderView } from "./renderer.ts";
import { kCellSize, kMinGridSize, kMaxGridSize, validGridSize } from "./grid.ts";
import { ProjectSession, type SaveStatus } from "./project-session.ts";
import { mapPoint, hitScaleHandle, placementScale, type Point } from "./props.ts";

let editor: Editor;
let ready = false;
let fileBusy = false;
const assets = new Set<TileAtlas | SpriteSequence>();
let hoverClient: Point | null = null;
let activeScaleCursor = "";
let scalePanelPlacementId: number | null = null;
let scalePanelBase = 1;
let scaleEditMode: "absolute" | "percent" = "absolute";
let overlayFrame: number | null = null;
let viewportFrame: number | null = null;
let renderedView: RenderView | null = null;

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

/// Pointer and scroll events can request several updates before the next display frame.
function drawPropOverlay(): void
{
    if(!ready || overlayFrame !== null) { return; }
    overlayFrame = requestAnimationFrame(() => {
        overlayFrame = null;
        renderPropOverlay();
    });
}

function renderPropOverlay(): void
{
    if(overlayFrame !== null) { cancelAnimationFrame(overlayFrame); overlayFrame = null; }
    let preview: ImagePreview | null = null;
    const asset = editor.tool === "place" ? editor.selectedDefinition?.sprite.sequences[0] : editor.tool === "terrain" ? editor.getLayer(editor.document.activeLayerId).sheet : null;
    if(asset && hoverClient && editor.activeGesture?.kind !== "pan" && !fileBusy
        && !dialogOpen())
    {
        const bounds = required("map-surface", HTMLDivElement).getBoundingClientRect();
        const point = mapPoint(hoverClient.x, hoverClient.y, bounds, editor.document.gridSize);
        const size = editor.document.gridSize;
        if(Math.abs(point.x) <= size / 2 && Math.abs(point.y) <= size / 2)
        {
            if(editor.tool === "terrain")
            {
                const thumbnail = controls.palette.querySelector<HTMLElement>('[aria-pressed="true"] .material-preview');
                if(thumbnail)
                {
                    const rect = thumbnail.getBoundingClientRect();
                    // A collapsed palette has no layout box, but retains its responsive CSS dimensions.
                    const style = getComputedStyle(thumbnail);
                    const width = rect.width || Number.parseFloat(style.width);
                    const height = rect.height || Number.parseFloat(style.height);
                    preview = { asset, scale: 1, ...point, size: {
                        width: width * size * kCellSize / bounds.width,
                        height: height * size * kCellSize / bounds.height
                    } };
                }
            }
            else if(editor.selectedDefinition)
            {
                preview = { asset, scale: editor.selectedDefinition.scale, ...point, actor: editor.selectedDefinition.kind === "actor" };
            }
        }
    }
    drawObjects(editor.document, required("prop-overlay", HTMLCanvasElement), renderedView ?? layoutViewport(), editor.selectedPlacementIds, preview, editor.selectionBox, required("show-collision-rects", HTMLInputElement).checked);
}

function updatePropControls(): void
{
    const selected = editor.selectedPlacementIds.size === 1 ? editor.document.placements.find((prop) => editor.selectedPlacementIds.has(prop.id)) : undefined;
    if(scalePanelPlacementId !== null && (selected?.id !== scalePanelPlacementId || editor.tool !== "select")) { closeScalePanel(); }
    const draggingScale = editor.activeGesture?.kind === "scale";
    required("scale-panel", HTMLFormElement).dataset.dragging = String(draggingScale);
    required("scale-value", HTMLInputElement).readOnly = draggingScale;
    required("scale-percent", HTMLInputElement).readOnly = draggingScale;
    if(draggingScale && selected && scalePanelPlacementId === selected.id)
    {
        updateScaleValues(placementScale(selected), true);
    }
    required("reset-scale", HTMLButtonElement).disabled = !selected || selected.scale === undefined || editor.activeGesture !== null;
    required("prop-selection", HTMLOutputElement).value = (selected ? `${selected.definition.name} (${selected.definition.kind === "actor" ? selected.definition.class : "prop"})` : undefined)
        ?? (editor.selectedPlacementIds.size > 1 ? `${editor.selectedPlacementIds.size} objects selected` : "No object selected");
}

/// Percentages share the scale captured when the handle was pressed or clicked.
function updateScaleValues(scale: number, round: boolean): void
{
    const format = (value: number): string => String(round ? Number(value.toPrecision(4)) : value);
    required("scale-value", HTMLInputElement).value = format(scale);
    required("scale-percent", HTMLInputElement).value = format(scale / scalePanelBase * 100);
}

function closeScalePanel(): void
{
    const panel = required("scale-panel", HTMLFormElement);
    const hadFocus = panel.contains(document.activeElement);
    panel.hidden = true;
    scalePanelPlacementId = null;
    if(hadFocus) { controls.map.focus({ preventScroll: true }); }
}

function openScalePanel(client: Point, focusInput: boolean): void
{
    if(editor.selectedPlacementIds.size !== 1 || editor.tool !== "select") { return; }
    const selected = editor.document.placements.find((item) => editor.selectedPlacementIds.has(item.id));
    if(!selected) { return; }
    scalePanelPlacementId = selected.id;
    const panel = required("scale-panel", HTMLFormElement);
    const input = required("scale-value", HTMLInputElement);
    panel.setAttribute("aria-label", `${selected.definition.name} scale`);
    scalePanelBase = placementScale(selected);
    scaleEditMode = "absolute";
    updateScaleValues(scalePanelBase, false);
    input.setCustomValidity("");
    required("scale-percent", HTMLInputElement).setCustomValidity("");
    panel.hidden = false;
    positionScalePanel(client);
    updatePropControls();
    if(focusInput)
    {
        input.focus({ preventScroll: true });
        input.select();
    }
}

function positionScalePanel(client: Point): void
{
    const panel = required("scale-panel", HTMLFormElement);
    if(panel.hidden) { return; }
    const bounds = controls.mapViewport.getBoundingClientRect();
    panel.style.maxWidth = `${Math.max(1, controls.mapViewport.clientWidth - 16)}px`;
    panel.style.maxHeight = `${Math.max(1, controls.mapViewport.clientHeight - 16)}px`;
    panel.style.left = `${Math.max(bounds.left + 8, Math.min(client.x + 12,
        bounds.left + controls.mapViewport.clientWidth - panel.offsetWidth - 8))}px`;
    panel.style.top = `${Math.max(bounds.top + 8, Math.min(client.y + 12,
        bounds.top + controls.mapViewport.clientHeight - panel.offsetHeight - 8))}px`;
}

function renderObjectPalette(id: string, definitions: readonly ObjectDefinition[]): void
{
    const palette = required(id, HTMLDivElement);
    palette.replaceChildren();
    for(const definition of definitions)
    {
        const asset = definition.sprite.sequences[0];
        const button = document.createElement("button");
        button.type = "button";
        button.className = "material-choice";
        const source = propFrameSource(asset);
        button.title = `${definition.name} — ${asset.id} (${source.w}×${source.h})`;
        button.setAttribute("aria-label", definition.name);
        button.setAttribute("aria-pressed", String(editor.tool === "place" && definition === editor.selectedDefinition));
        const preview = document.createElement("canvas");
        preview.width = 56;
        preview.height = 56;
        preview.setAttribute("aria-hidden", "true");
        preview.className = "prop-preview";
        const context = preview.getContext("2d");
        if(context === null) { throw new Error("Canvas 2D is unavailable"); }
        context.imageSmoothingEnabled = false;
        const scale = Math.min(preview.width / source.w, preview.height / source.h);
        const width = source.w * scale, height = source.h * scale;
        context.drawImage(source.image, source.x, source.y, source.w, source.h,
            (preview.width - width) / 2, (preview.height - height) / 2, width, height);
        button.append(preview);
        button.addEventListener("click", () => {
            finishInteraction("commit");
            editor.chooseDefinition(definition);
            refreshTool();
        });
        palette.append(button);
    }
    if(definitions.length === 0)
    {
        palette.textContent = "Add definitions to level.json and reload the project.";
    }
}

function refreshTool(): void
{
    controls.map.dataset.tool = editor.tool;
    renderPalette();
    renderObjectPalette("prop-palette", editor.document.propTypes);
    renderObjectPalette("actor-palette", editor.document.actorTypes);
    updatePropControls();
    drawPropOverlay();
}

function setTool(next: Tool): void
{
    finishInteraction("commit");
    editor.setTool(next);
    refreshTool();
}

function propPoint(event: PointerEvent): Point
{
    return mapPoint(event.clientX, event.clientY, required("map-surface", HTMLDivElement).getBoundingClientRect(), editor.document.gridSize);
}

function clientPoint(event: PointerEvent): Point { return { x: event.clientX, y: event.clientY }; }

/// The surface supplies scroll extent and input coordinates, never pixel storage.
function layoutViewport(): RenderView
{
    const viewport = controls.mapViewport;
    const zoom = editor.document.view.zoom;
    const size = editor.document.gridSize * kCellSize * zoom;
    const left = Math.min(viewport.scrollLeft, Math.max(0, size - viewport.clientWidth));
    const top = Math.min(viewport.scrollTop, Math.max(0, size - viewport.clientHeight));
    const width = Math.max(1, Math.ceil(Math.min(viewport.clientWidth, size - left)));
    const height = Math.max(1, Math.ceil(Math.min(viewport.clientHeight, size - top)));
    for(const element of [controls.map, required("prop-overlay", HTMLCanvasElement), controls.grid])
    {
        element.style.left = `${left}px`;
        element.style.top = `${top}px`;
        element.style.width = `${width}px`;
        element.style.height = `${height}px`;
    }
    controls.grid.setAttribute("viewBox", `${left / zoom} ${top / zoom} ${width / zoom} ${height / zoom}`);
    return { x: left / zoom, y: top / zoom, width, height, zoom };
}

function coverageView(): RenderView
{
    const width = Math.max(1, Math.floor(controls.coverage.getBoundingClientRect().width));
    return { x: 0, y: 0, width, height: width, zoom: width / (editor.document.gridSize * kCellSize) };
}

function drawCoverage(startX: number, startY: number, endX: number, endY: number): void
{
    drawCoverageRegion(editor.document, controls.coverage, coverageView(), startX, startY, endX, endY);
}

function drawMapRegion(startX: number, startY: number, endX: number, endY: number): void
{
    const view = layoutViewport();
    if(!renderedView || view.x !== renderedView.x || view.y !== renderedView.y
        || view.width !== renderedView.width || view.height !== renderedView.height || view.zoom !== renderedView.zoom)
    {
        startX = 0; startY = 0; endX = editor.document.gridSize; endY = editor.document.gridSize;
    }
    drawTerrainRegion(editor.document, controls.map, view, startX, startY, endX, endY);
    renderedView = view;
}

function drawRegion(startX: number, startY: number, endX: number, endY: number): void
{
    drawMapRegion(startX, startY, endX, endY);
    drawCoverage(startX, startY, endX, endY);
}

function draw(): void
{
    drawRegion(0, 0, editor.document.gridSize, editor.document.gridSize);
    drawPropOverlay();
}

/// Coalesce scrolling and layout changes into one redraw per animation frame.
function requestViewportDraw(): void
{
    if(!ready || viewportFrame !== null) { return; }
    viewportFrame = requestAnimationFrame(() => {
        viewportFrame = null;
        drawMapRegion(0, 0, editor.document.gridSize, editor.document.gridSize);
        if(controls.coverage.width !== coverageView().width)
        {
            drawCoverage(0, 0, editor.document.gridSize, editor.document.gridSize);
        }
        renderPropOverlay();
    });
}

/// Keep the grid vector-based so terrain scaling cannot discard thin lines.
function initializeGrid(): void
{
    const size = editor.document.gridSize * kCellSize;
    controls.grid.setAttribute("viewBox", `0 0 ${size} ${size}`);
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    const segments: string[] = [];
    for(let cell = 0; cell <= editor.document.gridSize; cell++)
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
    const fade = Math.max(0, Math.min(1, (kCellSize * editor.document.view.zoom - 4) / 8));
    controls.grid.style.opacity = String(controls.showGrid.checked ? 0.58 * fade : 0);
}

function cellAt(event: PointerEvent): number
{
    const bounds = required("map-surface", HTMLDivElement).getBoundingClientRect();
    const tileX = Math.floor((event.clientX - bounds.left) * editor.document.gridSize / bounds.width);
    const tileY = Math.floor((event.clientY - bounds.top) * editor.document.gridSize / bounds.height);
    if(tileX < 0 || tileX >= editor.document.gridSize || tileY < 0 || tileY >= editor.document.gridSize)
    {
        return -1;
    }
    return tileY * editor.document.gridSize + tileX;
}

function updateInteraction(event: PointerEvent): void
{
    const gesture = editor.activeGesture;
    if(!gesture || gesture.pointerId !== event.pointerId) { return; }
    switch(gesture.kind)
    {
    case "paint": {
        const cell = cellAt(event);
        const region = editor.paintTo(event.pointerId, cell);
        if(region)
        {
            const layer = editor.getLayer(editor.document.activeLayerId);
            const position = `${cell % editor.document.gridSize + 1}, ${Math.floor(cell / editor.document.gridSize) + 1}`;
            setMessage(controls.status, !layer.painted[cell]
                ? `${layer.sheet.label}: cleared ${position}`
                : `${layer.sheet.label}: ${layer === editor.document.layers[0] ? "revealed" : "occupied"} ${position}; higher layers cleared`, false);
            drawRegion(region.left, region.top, region.right, region.bottom);
        }
        break;
    }
    case "scale":
        editor.scaleTo(event.pointerId, propPoint(event), clientPoint(event));
        updatePropControls();
        positionScalePanel(clientPoint(event));
        drawPropOverlay();
        break;
    case "move":
    case "box":
        editor.moveSelection(event.pointerId, propPoint(event), clientPoint(event));
        updatePropControls();
        drawPropOverlay();
        break;
    case "pan": {
        const viewport = controls.mapViewport;
        const next = editor.panTo(event.pointerId, clientPoint(event), { x: viewport.scrollLeft, y: viewport.scrollTop });
        if(next) { viewport.scrollLeft = next.x; viewport.scrollTop = next.y; }
        break;
    }
    }
}

function startInteraction(event: PointerEvent): void
{
    if(!ready || fileBusy || dialogOpen() || editor.activeGesture) { return; }
    if(event.button !== 1 && event.target !== controls.map) { return; }
    if(event.button !== 0 && event.button !== 1 && event.button !== 2) { return; }
    event.preventDefault();
    controls.map.focus({ preventScroll: true });
    try
    {
        if(event.button === 1)
        {
            editor.beginPan(event.pointerId, clientPoint(event));
            controls.mapViewport.setPointerCapture(event.pointerId);
            controls.mapViewport.classList.add("panning");
        }
        else if(event.button === 2 && editor.tool === "place") { setTool("select"); }
        else if(editor.tool === "terrain")
        {
            editor.beginPaint(event.pointerId, event.button === 2);
            updateInteraction(event);
            if(event.button === 2) { finishInteraction("commit"); }
            else { controls.map.setPointerCapture(event.pointerId); }
        }
        else if(event.button === 0)
        {
            if(editor.tool === "place") { applyEdit(() => editor.placeObject(propPoint(event))); }
            else
            {
                if(event.shiftKey || !editor.beginScale(event.pointerId, propPoint(event), clientPoint(event), editor.document.view.zoom))
                {
                    editor.beginSelection(event.pointerId, propPoint(event), clientPoint(event), event.shiftKey);
                }
                else
                {
                    activeScaleCursor = scaleCursorAt(event);
                    controls.map.style.cursor = activeScaleCursor;
                    openScalePanel(clientPoint(event), false);
                }
                if(editor.activeGesture) { controls.map.setPointerCapture(event.pointerId); }
                updatePropControls();
            }
        }
    }
    catch(error) { finishInteraction("cancel"); showError(error); }
    drawPropOverlay();
}

function scaleCursorAt(event: PointerEvent): string
{
    if(editor.tool !== "select" || event.shiftKey || editor.selectedPlacementIds.size !== 1) { return ""; }
    const selected = editor.document.placements.find((item) => editor.selectedPlacementIds.has(item.id));
    return selected ? hitScaleHandle(selected, propPoint(event), editor.document.view.zoom)?.cursor ?? "" : "";
}

function moveInteraction(event: PointerEvent): void
{
    if(!ready) { return; }
    hoverClient = event.pointerType === "touch" ? null : clientPoint(event);
    const gesture = editor.activeGesture;
    if(gesture?.pointerId === event.pointerId)
    {
        if((event.buttons & (gesture.kind === "pan" ? 4 : 1)) === 0) { finishInteraction("commit"); }
        else { updateInteraction(event); }
    }
    controls.map.style.cursor = editor.activeGesture?.kind === "scale" ? activeScaleCursor : scaleCursorAt(event);
    if(editor.tool !== "select") { drawPropOverlay(); }
}

function endInteraction(event: PointerEvent): void
{
    if(!ready || editor.activeGesture?.pointerId !== event.pointerId) { return; }
    if(event.type === "pointerup") { updateInteraction(event); }
    const clickedScaleHandle = finishInteraction(event.type === "pointerup" ? "commit" : "cancel");
    if(event.type === "pointerup" && clickedScaleHandle) { openScalePanel(clientPoint(event), true); }
}

/// All exits clear controller state before releasing capture, which itself emits an event.
function finishInteraction(outcome: "commit" | "cancel" | "escape"): boolean
{
    if(!ready) { return false; }
    const gesture = editor.activeGesture;
    if(!gesture && outcome !== "escape") { return false; }
    let clickedScaleHandle = false;
    const previous = editor.document;
    try
    {
        const result = outcome === "escape" ? editor.escape() : editor.finishGesture(outcome);
        if(result?.changed) { session.changed(); }
        clickedScaleHandle = result?.clickedScaleHandle ?? false;
        if(gesture?.kind === "scale")
        {
            if(outcome !== "commit") { closeScalePanel(); }
            else
            {
                const selected = editor.document.placements.find((item) => item.id === scalePanelPlacementId);
                if(selected) { updateScaleValues(placementScale(selected), false); }
            }
        }
    }
    catch(error) { closeScalePanel(); showError(error); }
    finally
    {
        if(gesture)
        {
            const target = gesture.kind === "pan" ? controls.mapViewport : controls.map;
            if(target.hasPointerCapture(gesture.pointerId)) { target.releasePointerCapture(gesture.pointerId); }
        }
        controls.mapViewport.classList.remove("panning");
        controls.map.style.cursor = "";
        activeScaleCursor = "";
    }
    if(previous !== editor.document) { refreshDocument(previous); }
    else { updatePropControls(); drawPropOverlay(); updateProjectButtons(); }
    if(outcome === "escape") { refreshTool(); }
    collectAssets();
    return clickedScaleHandle;
}

/// Retain the map point under the cursor while changing display scale.
function zoomMap(event: WheelEvent): void
{
    if(!ready || fileBusy || dialogOpen() || !event.altKey) { return; }
    event.preventDefault();
    if(editor.activeGesture !== null)
    {
        return;
    }
    const viewport = controls.mapViewport;
    const unit = event.deltaMode === WheelEvent.DOM_DELTA_LINE ? 16
        : event.deltaMode === WheelEvent.DOM_DELTA_PAGE ? viewport.clientHeight : 1;
    const nextZoom = Math.max(1 / 16, Math.min(4, editor.document.view.zoom * Math.exp(-event.deltaY * unit * 0.002)));
    const bounds = required("map-surface", HTMLDivElement).getBoundingClientRect();
    const ratio = nextZoom / editor.document.view.zoom;
    const offsetX = event.clientX - bounds.left;
    const offsetY = event.clientY - bounds.top;
    const scrollLeft = viewport.scrollLeft;
    const scrollTop = viewport.scrollTop;
    const size = editor.document.gridSize * kCellSize * nextZoom;
    required("map-surface", HTMLDivElement).style.width = `${size}px`;
    required("map-surface", HTMLDivElement).style.height = `${size}px`;
    viewport.scrollLeft = scrollLeft + offsetX * (ratio - 1);
    viewport.scrollTop = scrollTop + offsetY * (ratio - 1);
    editor.document.view.zoom = nextZoom;
    requestViewportDraw();
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
    editor.document.layers.forEach((layer) => {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "material-choice";
        button.setAttribute("aria-pressed", String(editor.tool === "terrain" && layer.id === editor.document.activeLayerId));
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
    finishInteraction("commit");
    if(editor.chooseLayer(id)) { session.changed(); }
    refreshTool();
    setMessage(controls.status, editor.getLayer(id) === editor.document.layers[0]
        ? "Base selected. Left-click or drag to clear overlays and reveal the background."
        : `${editor.getLayer(id).sheet.label} selected. Left-click or drag to fill and clear higher layers, right-click to clear this layer.`, false);
    drawCoverage(0, 0, editor.document.gridSize, editor.document.gridSize);
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
    editor.document.layers.forEach((layer, index) => {
        const id = layer.id;
        const card = document.createElement("div");
        card.className = "layer-card";
        const heading = document.createElement("strong");
        heading.textContent = index === 0 ? "Base layer" : `Layer ${index}`;
        const materialLabel = document.createElement("label");
        materialLabel.textContent = "Material";
        const choices = availableTiles();
        const materialSelect = makeSelect(choices.map((atlas) => atlas.label), choices.indexOf(layer.sheet));
        materialSelect.title = "Choose the tile sheet for this layer.";
        materialSelect.addEventListener("change", () => applyEdit(() =>
            editor.changeLayerMaterial(id, choices[Number(materialSelect.value)])));
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
            up.addEventListener("click", () => applyEdit(() => editor.moveLayer(id, -1)));
            const down = document.createElement("button");
            down.type = "button";
            down.textContent = "Move down";
            down.title = "Move this layer later in the list, above the following overlay.";
            down.disabled = index === editor.document.layers.length - 1;
            down.addEventListener("click", () => applyEdit(() => editor.moveLayer(id, 1)));
            const remove = document.createElement("button");
            remove.type = "button";
            remove.textContent = "Remove";
            remove.title = "Remove this layer and its painted cells. This edit can be undone.";
            remove.addEventListener("click", () => applyEdit(() => editor.removeLayer(id)));
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
        if(editor.document.layers.length >= 256) { throw new Error("Projects support at most 256 layers"); }
        const choices = availableTiles();
        const sheet = choices[Math.min(2, choices.length - 1)];
        const layer: Layer = {
            id: editor.document.nextLayerId++, painted: Array((editor.document.gridSize * editor.document.gridSize)).fill(false),
            sheet
        };
        editor.document.layers.push(layer);
        editor.document.activeLayerId = layer.id;
    });
}

function resetSample(): void
{
    edit("Load sample", () => {
        const choices = availableTiles();
        const makeLayer = (index: number): Layer => {
            const sheet = choices[Math.min(index, choices.length - 1)];
            const id = editor.document.nextLayerId++;
            return { id, sheet,
                painted: Array((editor.document.gridSize * editor.document.gridSize)).fill(index === 0) };
        };
        editor.document.layers = [makeLayer(0), makeLayer(1), makeLayer(2)];
        const full = editor.document.layers[1];
        const patch = editor.document.layers[2];
        for(let y = 0; y < editor.document.gridSize; y++)
        {
            for(let x = 0; x < editor.document.gridSize; x++) { full.painted[y * editor.document.gridSize + x] = true; }
        }
        for(let y = 1; y < Math.min(4, editor.document.gridSize); y++)
        {
            for(let x = 1; x < Math.min(4, editor.document.gridSize); x++) { patch.painted[y * editor.document.gridSize + x] = true; }
        }
        if(editor.document.gridSize >= 4) { patch.painted[3 * editor.document.gridSize + 2] = false; }
        if(editor.document.gridSize >= 2) { patch.painted[editor.document.gridSize + 1] = false; }
        editor.document.activeLayerId = patch.id;
    });
}

function dialogOpen(): boolean
{
    return required("project-dialog", HTMLDialogElement).open || required("resize-dialog", HTMLDialogElement).open;
}

let pendingMapSize: number | null = null;

function updateMapSizeInput(): void
{
    const input = required("map-size", HTMLSelectElement);
    const size = Number(input.value);
    required("map-size-hint", HTMLOutputElement).value = validGridSize(size)
        ? `${size * kCellSize} × ${size * kCellSize} pixels · cells remain ${kCellSize} × ${kCellSize} pixels`
        : `Choose a power of two from ${kMinGridSize} to ${kMaxGridSize}.`;
    required("map-size-apply", HTMLButtonElement).disabled = !ready || (validGridSize(size) && size === editor.document.gridSize);
}

function requestMapResize(event: SubmitEvent): void
{
    event.preventDefault();
    if(!ready || fileBusy || dialogOpen()) { return; }
    const input = required("map-size", HTMLSelectElement);
    const size = Number(input.value);
    if(!input.reportValidity() || !validGridSize(size) || size === editor.document.gridSize) { return; }
    finishInteraction("commit");
    const oldSize = editor.document.gridSize;
    pendingMapSize = size;
    const cropped = editor.document.placements.filter((prop) => Math.abs(prop.x) > size / 2 || Math.abs(prop.y) > size / 2);
    const removedProps = cropped.filter((placement) => placement.definition.kind === "prop").length;
    const removedActors = cropped.length - removedProps;
    required("resize-dimensions", HTMLParagraphElement).textContent =
        `Change from ${oldSize} × ${oldSize} cells (${oldSize * kCellSize} × ${oldSize * kCellSize} pixels) to ${size} × ${size} cells (${size * kCellSize} × ${size * kCellSize} pixels).`;
    required("resize-consequences", HTMLParagraphElement).textContent = size > oldSize
        ? "The map center stays fixed. New space is added equally on all four sides, filled with base terrain and empty overlays. Existing objects keep their coordinates. Terrain texture repeats and mask variations are regenerated for the resized grid."
        : `The map center stays fixed. Terrain is cropped equally from all four sides of every layer. ${removedProps} props and ${removedActors} actors will be removed because their pivots lie outside the new map. Remaining objects keep their coordinates. Terrain texture repeats and mask variations are regenerated for the resized grid. Objects with pivots on the boundary are kept, with artwork clipped at the map edge.`;
    hoverClient = null;
    required("resize-dialog", HTMLDialogElement).showModal();
    required("resize-cancel", HTMLButtonElement).focus();
    drawPropOverlay();
}

for(let size = kMinGridSize; size <= kMaxGridSize; size *= 2)
{
    const option = document.createElement("option");
    option.value = String(size);
    option.textContent = `${size} × ${size}`;
    required("map-size", HTMLSelectElement).append(option);
}
required("map-size", HTMLSelectElement).addEventListener("input", updateMapSizeInput);
required("map-size-form", HTMLFormElement).addEventListener("submit", requestMapResize);
required("resize-cancel", HTMLButtonElement).addEventListener("click", () => {
    pendingMapSize = null;
    required("resize-dialog", HTMLDialogElement).close();
});
required("resize-dialog", HTMLDialogElement).addEventListener("cancel", () => { pendingMapSize = null; });
required("resize-dialog", HTMLDialogElement).addEventListener("close", () => { pendingMapSize = null; });
required("resize-confirm", HTMLButtonElement).addEventListener("click", () => {
    const size = pendingMapSize;
    pendingMapSize = null;
    required("resize-dialog", HTMLDialogElement).close();
    if(size !== null) { applyEdit(() => editor.resizeMap(size)); }
});

controls.mapViewport.addEventListener("pointerdown", startInteraction);
controls.mapViewport.addEventListener("pointermove", moveInteraction);
controls.mapViewport.addEventListener("pointerup", endInteraction);
controls.mapViewport.addEventListener("pointercancel", endInteraction);
controls.mapViewport.addEventListener("lostpointercapture", endInteraction);
controls.mapViewport.addEventListener("auxclick", (event) => {
    if(event.button === 1)
    {
        event.preventDefault();
    }
});
controls.mapViewport.addEventListener("wheel", zoomMap, { passive: false });
controls.map.addEventListener("contextmenu", (event) => event.preventDefault());
controls.addLayer.addEventListener("click", addOverlay);
controls.activeMask.addEventListener("change", () => edit("Change mask mode", () => {
    editor.document.maskMode = controls.activeMask.value === "random" ? "random" : Number(controls.activeMask.value);
}));
required("show-collision-rects", HTMLInputElement).addEventListener("change", drawPropOverlay);
controls.showGrid.addEventListener("change", () => { updateGrid(); saveView(); });
controls.mapViewport.addEventListener("scroll", () => {
    closeScalePanel();
    saveView();
    requestViewportDraw();
});
controls.mapViewport.addEventListener("pointerleave", () => {
    hoverClient = null;
    if(ready) { drawPropOverlay(); }
});
new ResizeObserver(() => { closeScalePanel(); requestViewportDraw(); }).observe(controls.mapViewport);
new ResizeObserver(requestViewportDraw).observe(controls.coverage.parentElement!);
globalThis.addEventListener("blur", () => {
    hoverClient = null;
    if(ready) { finishInteraction("cancel"); drawPropOverlay(); }
});
document.addEventListener("pointerdown", (event) => {
    if(scalePanelPlacementId !== null && event.target instanceof Node
        && !required("scale-panel", HTMLFormElement).contains(event.target)) { closeScalePanel(); }
}, true);
globalThis.addEventListener("resize", closeScalePanel);
for(const mode of ["absolute", "percent"] as const)
{
    const input = required(mode === "absolute" ? "scale-value" : "scale-percent", HTMLInputElement);
    const other = required(mode === "absolute" ? "scale-percent" : "scale-value", HTMLInputElement);
    input.addEventListener("input", () => {
        scaleEditMode = mode;
        input.setCustomValidity("");
        other.setCustomValidity("");
        const value = input.valueAsNumber;
        const converted = mode === "absolute" ? value / scalePanelBase * 100 : scalePanelBase * (value / 100);
        other.value = Number.isFinite(converted) && converted > 0 ? String(converted) : "";
    });
    input.addEventListener("keydown", (event) => {
        if(event.key !== "Enter") { return; }
        event.preventDefault();
        scaleEditMode = mode;
        required("scale-panel", HTMLFormElement).requestSubmit();
    });
}
required("scale-panel", HTMLFormElement).addEventListener("submit", (event) => {
    event.preventDefault();
    if(!ready || fileBusy || dialogOpen() || scalePanelPlacementId === null) { return; }
    const input = required(scaleEditMode === "absolute" ? "scale-value" : "scale-percent", HTMLInputElement);
    const value = input.valueAsNumber;
    input.setCustomValidity(Number.isFinite(value) && value > 0 ? "" : "Enter a finite number greater than zero.");
    if(!input.reportValidity()) { return; }
    const scale = scaleEditMode === "absolute" ? value : scalePanelBase * (value / 100);
    applyEdit(() => {
        const result = editor.setScale(scale, "absolute");
        closeScalePanel();
        return result;
    });
});
required("reset-scale", HTMLButtonElement).addEventListener("click", () => {
    if(!ready || fileBusy || dialogOpen() || scalePanelPlacementId === null) { return; }
    applyEdit(() => {
        const result = editor.resetScale();
        closeScalePanel();
        return result;
    });
});
required("reset", HTMLButtonElement).addEventListener("click", resetSample);

function renderMaskChoices(): void
{
    controls.activeMask.replaceChildren(new Option("Random per junction", "random"));
    const links = required("mask-links", HTMLParagraphElement);
    links.replaceChildren();
    editor.document.masks.forEach((mask, index) => {
        controls.activeMask.add(new Option(mask.label, String(index)));
        const link = document.createElement("a");
        link.href = mask.url;
        link.textContent = mask.label;
        if(index > 0) { links.append(" · "); }
        links.append(link);
    });
    controls.activeMask.value = String(editor.document.maskMode);
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

const session = new ProjectSession(() => editor.committedDocument, writeProject, updateSaveStatus, 600);

function availableTiles(): TileAtlas[]
{
    // Preserve directory-listing order in material controls without storing another catalog.
    return editor.document.tileSheets.toSorted((a, b) => {
        const aPath = a.id.split("/").map(encodeURIComponent).join("/");
        const bPath = b.id.split("/").map(encodeURIComponent).join("/");
        return aPath < bPath ? -1 : aPath > bPath ? 1 : 0;
    });
}

function collectAssets(): void
{
    const retained = new Set([...(ready ? documentAssets(editor.document) : []), ...(ready ? editor.history.retainedStates().flatMap(documentAssets) : [])]);
    const retainedUrls = new Set([...retained].map((asset) => asset.url));
    const released = new Set<string>();
    for(const asset of assets)
    {
        if(!retained.has(asset))
        {
            if(!retainedUrls.has(asset.url) && !released.has(asset.url))
            {
                URL.revokeObjectURL(asset.url);
                released.add(asset.url);
            }
            assets.delete(asset);
        }
    }
}

/// Refresh controls against the current snapshot, but reuse unchanged terrain pixels.
/// Pass null when installing a project. Otherwise previous describes the rendered terrain.
function refreshDocument(previous: ProjectDocument | null): void
{
    controls.showGrid.checked = editor.document.view.showGrid;
    if(previous === null || previous.gridSize !== editor.document.gridSize) { initializeGrid(); }
    const dimensions = `${editor.document.gridSize} × ${editor.document.gridSize}`;
    required("map-heading", HTMLHeadingElement).textContent = `Tilemap · ${dimensions}`;
    controls.map.setAttribute("aria-label", `Editable ${dimensions} cell layered material tilemap`);
    controls.coverage.setAttribute("aria-label", `Selected layer alpha mask for the ${dimensions} cell tilemap`);
    required("map-size", HTMLSelectElement).value = String(editor.document.gridSize);
    updateMapSizeInput();
    const size = editor.document.gridSize * kCellSize * editor.document.view.zoom;
    required("map-surface", HTMLDivElement).style.width = `${size}px`;
    required("map-surface", HTMLDivElement).style.height = `${size}px`;
    controls.mapViewport.scrollLeft = editor.document.view.scrollLeft;
    controls.mapViewport.scrollTop = editor.document.view.scrollTop;
    renderMaskChoices();
    renderLayers();
    renderObjectPalette("prop-palette", editor.document.propTypes);
    renderObjectPalette("actor-palette", editor.document.actorTypes);
    controls.map.dataset.tool = editor.tool;
    updatePropControls();
    if(previous === null || !sameTerrainContent(previous, editor.document))
    {
        draw();
    }
    else
    {
        const previousLayer = previous.layers.findIndex((layer) => layer.id === previous.activeLayerId);
        const activeLayer = editor.document.layers.findIndex((layer) => layer.id === editor.document.activeLayerId);
        if(previousLayer !== activeLayer)
        {
            drawCoverage(0, 0, editor.document.gridSize, editor.document.gridSize);
        }
        drawPropOverlay();
    }
    updateGrid();
    updateProjectButtons();
}

function installDocument(next: ProjectDocument): void
{
    const nextEditor = new Editor(next, new History(100, 64 * 1024 * 1024));
    if(ready) { nextEditor.setTool(editor.tool); }
    editor = nextEditor;
    renderedView = null;
    hoverClient = null;
    ready = true;
    for(const asset of documentAssets(next)) { assets.add(asset); }
    required("editor", HTMLDivElement).inert = false;
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
    if(editor.document.view.scrollLeft !== left || editor.document.view.scrollTop !== top || editor.document.view.showGrid !== grid)
    {
        editor.document.view.scrollLeft = left;
        editor.document.view.scrollTop = top;
        editor.document.view.showGrid = grid;
        session.changed();
    }
}

function applyEdit(action: () => EditResult | null): void
{
    finishInteraction("commit");
    const previous = editor.document;
    let result: EditResult | null;
    try { result = action(); }
    catch(error) { showError(error); refreshDocument(previous); collectAssets(); return; }
    if(!result) { return; }
    if(result.changed) { session.changed(); }
    refreshDocument(result.previous);
    collectAssets();
}

function edit(label: string, mutate: () => void): void
{
    applyEdit(() => editor.edit(label, mutate));
}

function changeHistory(redo: boolean): void
{
    if(!ready || fileBusy || dialogOpen()) { return; }
    closeScalePanel();
    applyEdit(() => editor.changeHistory(redo));
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
    undo.disabled = !ready || editor.history.undoLabel === null;
    redo.disabled = !ready || editor.history.redoLabel === null;
    undo.textContent = !ready || editor.history.undoLabel === null ? "Undo" : `Undo ${editor.history.undoLabel}`;
    redo.textContent = !ready || editor.history.redoLabel === null ? "Redo" : `Redo ${editor.history.redoLabel}`;
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
    if(fileBusy || dialogOpen()) { return; }
    finishInteraction("commit");
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
    if(fileBusy || required("resize-dialog", HTMLDialogElement).open || (action === "open" && projectListing)) { return; }
    finishInteraction("commit");
    closeScalePanel();
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
    if(!ready || fileBusy || dialogOpen()) { return; }
    if(event.key === "Escape")
    {
        event.preventDefault();
        if(scalePanelPlacementId !== null)
        {
            if(editor.activeGesture) { finishInteraction("escape"); }
            closeScalePanel();
            return;
        }
        finishInteraction("escape");
        return;
    }
    if(scalePanelPlacementId !== null) { return; }
    const target = event.target;
    if(target instanceof HTMLElement && (target.isContentEditable || target.closest("input, textarea, select"))) { return; }
    if((event.key === "Delete" || event.key === "Backspace") && editor.selectedPlacementIds.size > 0
        && !event.ctrlKey && !event.metaKey && !event.altKey)
    {
        event.preventDefault();
        applyEdit(() => editor.deleteSelectedObjects());
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
    if(fileBusy || (ready && (session.dirty || editor.history.inTransaction)))
    {
        event.preventDefault();
        event.returnValue = "";
    }
});
globalThis.addEventListener("keydown", (event) => {
    if(!(event.ctrlKey || event.metaKey) || event.altKey || fileBusy
        || dialogOpen()) { return; }
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

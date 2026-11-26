import { validateDocument, type ProjectDocument, type Layer, type Placement, type ObjectDefinition, type ImageAsset } from "./document.ts";
import { History } from "./history.ts";
import { kMinGridSize, kMaxGridSize, validGridSize } from "./grid.ts";
import { clampPropPoint, hitProp, propsInBox, groupMoveDelta, type Point, type SelectionBox } from "./props.ts";

export type Tool = "terrain" | "place" | "select";
export type GestureOutcome = "commit" | "cancel";
export interface EditResult { readonly previous: ProjectDocument; readonly changed: boolean; }
export interface CellRegion { readonly left: number; readonly top: number; readonly right: number; readonly bottom: number; }

interface PaintGesture
{
    readonly kind: "paint";
    readonly pointerId: number;
    readonly layerId: number;
    readonly erase: boolean;
    lastCell: number;
}
interface SelectionGesture
{
    readonly pointerId: number;
    readonly start: Point;
    readonly clientStart: Point;
    readonly beforeSelection: Set<number>;
    moved: boolean;
}
interface MoveGesture extends SelectionGesture
{
    readonly kind: "move";
    readonly clickedId: number;
    // These references are borrowed only until the gesture ends. Replacement requires an idle editor.
    readonly positions: { readonly prop: Placement; readonly x: number; readonly y: number }[];
}
interface BoxGesture extends SelectionGesture
{
    readonly kind: "box";
    readonly baseSelection: Set<number>;
    end: Point;
}
interface PanGesture
{
    readonly kind: "pan";
    readonly pointerId: number;
    lastClient: Point;
}
type Gesture = PaintGesture | MoveGesture | BoxGesture | PanGesture;
export interface GestureResult extends EditResult { readonly pointerId: number; readonly kind: Gesture["kind"]; }

/// Owns editable state and gesture transactions. The caller handles input capture, rendering and saves.
export class Editor
{
    private project: ProjectDocument;
    private selection = new Set<number>();
    private mode: Tool = "select";
    private definition: ObjectDefinition | null;
    private gesture: Gesture | null = null;

    constructor(project: ProjectDocument, readonly history: History)
    {
        validateDocument(project);
        this.project = project;
        this.definition = project.propTypes[0] ?? project.actorTypes[0] ?? null;
    }

    get document(): ProjectDocument { return this.project; }
    get committedDocument(): ProjectDocument { return this.history.committedState(this.project); }
    get selectedPlacementIds(): ReadonlySet<number> { return this.selection; }
    get selectedDefinition(): ObjectDefinition | null { return this.definition; }
    get tool(): Tool { return this.mode; }
    get activeGesture(): { readonly kind: Gesture["kind"]; readonly pointerId: number } | null { return this.gesture; }
    get selectionBox(): SelectionBox | null
    {
        return this.gesture?.kind === "box" && this.gesture.moved
            ? { start: this.gesture.start, end: this.gesture.end } : null;
    }

    getLayer(id: number): Layer
    {
        const layer = this.project.layers.find((candidate) => candidate.id === id);
        if(!layer) { throw new Error(`Missing layer ${id}`); }
        return layer;
    }

    setTool(tool: Tool): void
    {
        this.requireIdle();
        this.mode = tool;
        if(tool !== "select") { this.selection.clear(); }
    }

    chooseDefinition(definition: ObjectDefinition): void
    {
        this.requireIdle();
        if(!(definition.kind === "prop" ? this.project.propTypes.includes(definition) : this.project.actorTypes.includes(definition))) { throw new Error("Definition is not in this project"); }
        this.definition = definition;
        this.setTool("place");
    }

    chooseLayer(id: number): boolean
    {
        this.requireIdle();
        this.getLayer(id);
        const changed = this.project.activeLayerId !== id;
        this.project.activeLayerId = id;
        this.setTool("terrain");
        return changed;
    }

    /// An immediate edit is atomic. Presentation and save notifications happen after this returns.
    edit(label: string, mutate: (project: ProjectDocument) => void): EditResult
    {
        this.requireIdle();
        const selection = new Set(this.selection);
        this.history.begin(this.project, label);
        const previous = this.committedDocument;
        try
        {
            mutate(this.project);
            validateDocument(this.project);
            const changed = this.history.commit(this.project);
            this.reconcileSelection();
            return { previous, changed };
        }
        catch(error)
        {
            const before = this.history.rollback();
            if(before) { this.restore(before); }
            this.selection = selection;
            this.reconcileSelection();
            throw error;
        }
    }

    /// Object positions and selection gestures use fractional cell coordinates.
    placeObject(point: Point): EditResult
    {
        return this.edit(this.definition?.kind === "actor" ? "Place actor" : "Place prop", (project) => {
            if(!this.definition) { throw new Error("Add a prop or actor definition to level.json and reload the project."); }
            if(project.placements.length >= 10000) { throw new Error("The level already contains 10,000 objects."); }
            const id = project.nextPlacementId++;
            const position = clampPropPoint(point.x, point.y, this.project.gridSize);
            const firstActor = project.placements.findIndex((placement) => placement.definition.kind === "actor");
            const index = this.definition.kind === "prop" && firstActor >= 0 ? firstActor : project.placements.length;
            project.placements.splice(index, 0, { id, definition: this.definition,
                x: Math.round(position.x * 1000) / 1000,
                y: Math.round(position.y * 1000) / 1000, rotation: 0 });
            this.selection = new Set([id]);
        });
    }

    deleteSelectedObjects(): EditResult
    {
        return this.edit(this.selection.size > 1 ? "Delete objects" : "Delete object", (project) => {
            project.placements = project.placements.filter((prop) => !this.selection.has(prop.id));
            this.selection.clear();
        });
    }

    changeLayerMaterial(id: number, sheet: ImageAsset): EditResult
    {
        return this.edit("Change material", () => { this.getLayer(id).sheet = sheet; });
    }

    moveLayer(id: number, direction: -1 | 1): EditResult
    {
        return this.edit(direction === -1 ? "Move layer up" : "Move layer down", (project) => {
            const layer = this.getLayer(id);
            const index = project.layers.indexOf(layer);
            const target = index + direction;
            if(index === 0 || target < 1 || target >= project.layers.length) { throw new Error("Cannot move layer there"); }
            [project.layers[index], project.layers[target]] = [project.layers[target], layer];
        });
    }

    removeLayer(id: number): EditResult
    {
        return this.edit("Remove layer", (project) => {
            const index = project.layers.indexOf(this.getLayer(id));
            if(index === 0) { throw new Error("The base layer cannot be removed"); }
            project.layers.splice(index, 1);
            if(project.activeLayerId === id) { project.activeLayerId = project.layers[index - 1].id; }
        });
    }

    /// Keep the map center fixed. Cropped cells and objects are retained by the history snapshot.
    resizeMap(size: number): EditResult
    {
        if(!validGridSize(size)) { throw new Error(`Map size must be a power of two from ${kMinGridSize} to ${kMaxGridSize} cells.`); }
        return this.edit("Resize map", (project) => {
            const oldSize = project.gridSize;
            if(size === oldSize) { return; }
            const overlap = Math.min(oldSize, size);
            const sourceOffset = (oldSize - overlap) / 2;
            const targetOffset = (size - overlap) / 2;
            project.layers = project.layers.map((layer, index) => {
                const painted: boolean[] = Array(size * size).fill(index === 0);
                for(let y = 0; y < overlap; y++)
                {
                    for(let x = 0; x < overlap; x++) { painted[(y + targetOffset) * size + x + targetOffset] = layer.painted[(y + sourceOffset) * oldSize + x + sourceOffset]; }
                }
                return { ...layer, painted };
            });
            project.placements = project.placements.filter((prop) => Math.abs(prop.x) <= size / 2 && Math.abs(prop.y) <= size / 2);
            project.gridSize = size;
        });
    }

    changeHistory(redo: boolean): EditResult | null
    {
        this.requireIdle();
        const previous = this.project;
        const restored = redo ? this.history.redo(previous) : this.history.undo(previous);
        if(!restored) { return null; }
        this.restore(restored);
        return { previous, changed: true };
    }

    beginPaint(pointerId: number, erase: boolean): void
    {
        this.requireIdle();
        const layerId = this.project.activeLayerId;
        if(erase && this.getLayer(layerId) === this.project.layers[0]) { throw new Error("The base cannot be cleared. Left-click to reveal it."); }
        this.history.begin(this.project, erase ? "Erase cell" : "Paint stroke");
        this.gesture = { kind: "paint", pointerId, layerId, erase, lastCell: -1 };
    }

    /// Return the affected cell bounds, including the border needed by junction masks.
    paintTo(pointerId: number, cell: number): CellRegion | null
    {
        const gesture = this.gesture;
        if(gesture?.kind !== "paint" || gesture.pointerId !== pointerId) { return null; }
        const gridSize = this.project.gridSize;
        if(cell < 0 || cell >= gridSize * gridSize) { gesture.lastCell = -1; return null; }
        if(cell === gesture.lastCell) { return null; }
        const layerIndex = this.project.layers.findIndex((layer) => layer.id === gesture.layerId);
        const layer = this.project.layers[layerIndex];
        const endX = cell % gridSize, endY = Math.floor(cell / gridSize);
        let x = gesture.lastCell < 0 ? endX : gesture.lastCell % gridSize;
        let y = gesture.lastCell < 0 ? endY : Math.floor(gesture.lastCell / gridSize);
        const startX = x, startY = y;
        const dx = Math.abs(endX - x), dy = -Math.abs(endY - y);
        const stepX = x < endX ? 1 : -1, stepY = y < endY ? 1 : -1;
        let error = dx + dy;
        for(;;)
        {
            const index = y * gridSize + x;
            layer.painted[index] = !gesture.erase;
            if(!gesture.erase)
            {
                for(let higher = layerIndex + 1; higher < this.project.layers.length; higher++)
                {
                    this.project.layers[higher].painted[index] = false;
                }
            }
            if(x === endX && y === endY) { break; }
            const twiceError = 2 * error;
            if(twiceError >= dy) { error += dy; x += stepX; }
            if(twiceError <= dx) { error += dx; y += stepY; }
        }
        gesture.lastCell = cell;
        return { left: Math.max(0, Math.min(startX, endX) - 1), top: Math.max(0, Math.min(startY, endY) - 1),
            right: Math.min(gridSize, Math.max(startX, endX) + 2), bottom: Math.min(gridSize, Math.max(startY, endY) + 2) };
    }

    /// Shift-click toggles immediately. Other clicks begin a move or box gesture.
    beginSelection(pointerId: number, point: Point, client: Point, shift: boolean): void
    {
        this.requireIdle();
        const prop = hitProp(this.project.placements, point.x, point.y);
        const beforeSelection = new Set(this.selection);
        if(prop && shift)
        {
            if(!this.selection.delete(prop.id)) { this.selection.add(prop.id); }
            return;
        }
        if(prop)
        {
            if(!this.selection.has(prop.id)) { this.selection = new Set([prop.id]); }
            this.history.begin(this.project, this.selection.size > 1 ? "Move objects" : "Move object");
            this.gesture = { kind: "move", pointerId, start: point, clientStart: client, beforeSelection,
                clickedId: prop.id, moved: false,
                positions: this.project.placements.filter((item) => this.selection.has(item.id)).map((item) => ({ prop: item, x: item.x, y: item.y })) };
        }
        else
        {
            if(!shift) { this.selection.clear(); }
            this.gesture = { kind: "box", pointerId, start: point, end: point, clientStart: client,
                beforeSelection, baseSelection: new Set(this.selection), moved: false };
        }
    }

    moveSelection(pointerId: number, point: Point, client: Point): void
    {
        const gesture = this.gesture;
        if(!gesture || (gesture.kind !== "move" && gesture.kind !== "box") || gesture.pointerId !== pointerId) { return; }
        gesture.moved ||= Math.hypot(client.x - gesture.clientStart.x, client.y - gesture.clientStart.y) >= 3;
        if(!gesture.moved) { return; }
        if(gesture.kind === "box")
        {
            gesture.end = clampPropPoint(point.x, point.y, this.project.gridSize);
            this.selection = new Set([...gesture.baseSelection, ...propsInBox(this.project.placements, gesture)]);
        }
        else
        {
            const delta = groupMoveDelta(gesture.positions, point.x - gesture.start.x, point.y - gesture.start.y, this.project.gridSize);
            for(const original of gesture.positions)
            {
                original.prop.x = Math.round((original.x + delta.x) * 1000) / 1000;
                original.prop.y = Math.round((original.y + delta.y) * 1000) / 1000;
            }
        }
    }

    beginPan(pointerId: number, client: Point): void
    {
        this.requireIdle();
        this.gesture = { kind: "pan", pointerId, lastClient: client };
    }

    panTo(pointerId: number, client: Point, scroll: Point): Point | null
    {
        const gesture = this.gesture;
        if(gesture?.kind !== "pan" || gesture.pointerId !== pointerId) { return null; }
        const next = { x: scroll.x + gesture.lastClient.x - client.x, y: scroll.y + gesture.lastClient.y - client.y };
        gesture.lastClient = client;
        return next;
    }

    /// Clear the gesture before the caller releases pointer capture. Repeated completion is harmless.
    /// Cancellation rolls back edits/selection. Panning simply stops, retaining the current view.
    finishGesture(outcome: GestureOutcome): GestureResult | null
    {
        const gesture = this.gesture;
        if(!gesture) { return null; }
        this.gesture = null;
        let previous = this.project;
        let changed = false;
        if(gesture.kind === "paint" || gesture.kind === "move")
        {
            if(outcome === "cancel")
            {
                const restored = this.history.rollback();
                if(restored) { this.restore(restored); }
            }
            else
            {
                previous = this.committedDocument;
                try { validateDocument(this.project); changed = this.history.commit(this.project); }
                catch(error)
                {
                    const restored = this.history.rollback();
                    if(restored) { this.restore(restored); }
                    if(gesture.kind === "move") { this.selection = gesture.beforeSelection; }
                    throw error;
                }
                if(gesture.kind === "move" && !gesture.moved) { this.selection = new Set([gesture.clickedId]); }
            }
        }
        if(outcome === "cancel" && (gesture.kind === "move" || gesture.kind === "box"))
        {
            this.selection = gesture.beforeSelection;
        }
        this.reconcileSelection();
        return { pointerId: gesture.pointerId, kind: gesture.kind, previous, changed };
    }

    escape(): GestureResult | null
    {
        const clearSelection = this.mode === "select" && this.gesture === null;
        const result = this.finishGesture("cancel");
        this.mode = "select";
        if(clearSelection) { this.selection.clear(); }
        return result;
    }

    /// Snapshot replacement preserves transient view and valid terrain selection.
    private restore(project: ProjectDocument): void
    {
        project.view = { ...this.project.view };
        if(project.layers.some((layer) => layer.id === this.project.activeLayerId))
        {
            project.activeLayerId = this.project.activeLayerId;
        }
        this.project = project;
        this.reconcileSelection();
    }

    private reconcileSelection(): void
    {
        this.selection = new Set(this.project.placements.filter((prop) => this.selection.has(prop.id)).map((prop) => prop.id));
        if(!this.definition || !(this.definition.kind === "prop"
            ? this.project.propTypes.includes(this.definition) : this.project.actorTypes.includes(this.definition)))
        {
            this.definition = this.project.propTypes[0] ?? this.project.actorTypes[0] ?? null;
        }
    }

    private requireIdle(): void
    {
        if(this.gesture !== null || this.history.inTransaction) { throw new Error("Finish the current gesture before editing"); }
    }
}

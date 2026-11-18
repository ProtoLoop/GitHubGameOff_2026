import { validateDocument, type ProjectDocument, type Layer, type Prop, type PropAsset, type ImageAsset } from "./document.ts";
import { History } from "./history.ts";
import { kGridSize, kCellCount } from "./grid.ts";
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
    readonly positions: { readonly prop: Prop; readonly x: number; readonly y: number }[];
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
    private mode: Tool = "terrain";
    private propAsset: PropAsset | null;
    private gesture: Gesture | null = null;

    constructor(project: ProjectDocument, readonly history: History)
    {
        validateDocument(project);
        this.project = project;
        this.propAsset = project.propAssets[0] ?? null;
    }

    get document(): ProjectDocument { return this.project; }
    get committedDocument(): ProjectDocument { return this.history.committedState(this.project); }
    get selectedPropIds(): ReadonlySet<number> { return this.selection; }
    get selectedPropAsset(): PropAsset | null { return this.propAsset; }
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

    chooseProp(asset: PropAsset): void
    {
        this.requireIdle();
        if(!this.project.propAssets.includes(asset)) { throw new Error("Prop image is not in this project"); }
        this.propAsset = asset;
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

    placeProp(point: Point): EditResult
    {
        return this.edit("Place prop", (project) => {
            if(!this.propAsset) { throw new Error("Add an image to assets/props/ and reload the project."); }
            if(project.props.length >= 10000) { throw new Error("The level already contains 10,000 props."); }
            const id = project.nextPropId++;
            const position = clampPropPoint(point.x, point.y);
            project.props.push({ id, asset: this.propAsset,
                x: Math.round(position.x * 1000) / 1000,
                y: Math.round(position.y * 1000) / 1000, rotation: 0 });
            this.selection = new Set([id]);
        });
    }

    deleteSelectedProps(): EditResult
    {
        return this.edit(this.selection.size > 1 ? "Delete props" : "Delete prop", (project) => {
            project.props = project.props.filter((prop) => !this.selection.has(prop.id));
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
        if(cell < 0 || cell >= kCellCount) { gesture.lastCell = -1; return null; }
        if(cell === gesture.lastCell) { return null; }
        const layerIndex = this.project.layers.findIndex((layer) => layer.id === gesture.layerId);
        const layer = this.project.layers[layerIndex];
        const endX = cell % kGridSize, endY = Math.floor(cell / kGridSize);
        let x = gesture.lastCell < 0 ? endX : gesture.lastCell % kGridSize;
        let y = gesture.lastCell < 0 ? endY : Math.floor(gesture.lastCell / kGridSize);
        const startX = x, startY = y;
        const dx = Math.abs(endX - x), dy = -Math.abs(endY - y);
        const stepX = x < endX ? 1 : -1, stepY = y < endY ? 1 : -1;
        let error = dx + dy;
        for(;;)
        {
            const index = y * kGridSize + x;
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
            right: Math.min(kGridSize, Math.max(startX, endX) + 2), bottom: Math.min(kGridSize, Math.max(startY, endY) + 2) };
    }

    /// Shift-click toggles immediately. Other clicks begin a move or box gesture.
    beginSelection(pointerId: number, point: Point, client: Point, shift: boolean): void
    {
        this.requireIdle();
        const prop = hitProp(this.project.props, point.x, point.y);
        const beforeSelection = new Set(this.selection);
        if(prop && shift)
        {
            if(!this.selection.delete(prop.id)) { this.selection.add(prop.id); }
            return;
        }
        if(prop)
        {
            if(!this.selection.has(prop.id)) { this.selection = new Set([prop.id]); }
            this.history.begin(this.project, this.selection.size > 1 ? "Move props" : "Move prop");
            this.gesture = { kind: "move", pointerId, start: point, clientStart: client, beforeSelection,
                clickedId: prop.id, moved: false,
                positions: this.project.props.filter((item) => this.selection.has(item.id)).map((item) => ({ prop: item, x: item.x, y: item.y })) };
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
            gesture.end = clampPropPoint(point.x, point.y);
            this.selection = new Set([...gesture.baseSelection, ...propsInBox(this.project.props, gesture)]);
        }
        else
        {
            const delta = groupMoveDelta(gesture.positions, point.x - gesture.start.x, point.y - gesture.start.y);
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
        this.selection = new Set(this.project.props.filter((prop) => this.selection.has(prop.id)).map((prop) => prop.id));
        if(!this.propAsset || !this.project.propAssets.includes(this.propAsset)) { this.propAsset = this.project.propAssets[0] ?? null; }
    }

    private requireIdle(): void
    {
        if(this.gesture !== null || this.history.inTransaction) { throw new Error("Finish the current gesture before editing"); }
    }
}

import { cloneDocument, documentBytes, sameContent, type ProjectDocument } from "./document.ts";

interface Entry { readonly label: string; readonly state: ProjectDocument; }

/// The controller explicitly begins and commits each user action, including whole strokes.
export class History
{
    private past: Entry[] = [];
    private future: Entry[] = [];
    private pending: Entry | null = null;

    constructor(private readonly maxActions: number, private readonly maxBytes: number) {}

    get undoLabel(): string | null { return this.past.at(-1)?.label ?? null; }
    get redoLabel(): string | null { return this.future.at(-1)?.label ?? null; }
    get inTransaction(): boolean { return this.pending !== null; }

    begin(project: ProjectDocument, label: string): void
    {
        if(this.pending !== null) { throw new Error("An edit is already in progress"); }
        this.pending = { label, state: cloneDocument(project) };
    }

    commit(project: ProjectDocument): boolean
    {
        const before = this.pending;
        this.pending = null;
        if(before === null || sameContent(before.state, project)) { return false; }
        this.past.push(before);
        this.future = [];
        this.trim();
        return true;
    }

    rollback(): ProjectDocument | null
    {
        const state = this.pending?.state ?? null;
        this.pending = null;
        return state;
    }

    undo(project: ProjectDocument): ProjectDocument | null
    {
        if(this.pending !== null) { throw new Error("Finish the current edit before undo"); }
        const entry = this.past.pop();
        if(!entry) { return null; }
        this.future.push({ label: entry.label, state: cloneDocument(project) });
        this.trim();
        return cloneDocument(entry.state);
    }

    redo(project: ProjectDocument): ProjectDocument | null
    {
        if(this.pending !== null) { throw new Error("Finish the current edit before redo"); }
        const entry = this.future.pop();
        if(!entry) { return null; }
        this.past.push({ label: entry.label, state: cloneDocument(project) });
        this.trim();
        return cloneDocument(entry.state);
    }

    committedState(project: ProjectDocument): ProjectDocument
    {
        return this.pending?.state ?? project;
    }

    clear(): void { this.past = []; this.future = []; this.pending = null; }

    retainedStates(): ProjectDocument[]
    {
        return [...this.past, ...this.future, ...(this.pending ? [this.pending] : [])].map((entry) => entry.state);
    }

    private trim(): void
    {
        while(this.past.length + this.future.length > this.maxActions
            || this.retainedStates().reduce((size, state) => size + documentBytes(state), 0) > this.maxBytes)
        {
            if(this.past.length > 0) { this.past.shift(); }
            else if(this.future.length > 0) { this.future.shift(); }
            else { break; }
        }
    }
}

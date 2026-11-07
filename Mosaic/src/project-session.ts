import { cloneDocument, type ProjectDocument } from "./document.ts";

export interface SaveStatus
{
    readonly filename: string | null;
    readonly dirty: boolean;
    readonly saving: boolean;
    readonly error: string | null;
}

/// Owns the HTTP target and one serialized save queue. No browser persistence.
export class ProjectSession
{
    private file: string | null = null;
    private revision = 0;
    private savedRevision = -1;
    private timer: ReturnType<typeof setTimeout> | null = null;
    private running: Promise<void> | null = null;
    private error: string | null = null;

    constructor(private readonly current: () => ProjectDocument,
        private readonly encode: (project: ProjectDocument) => Promise<Blob>,
        private readonly notify: (status: SaveStatus) => void,
        private readonly delayMs: number) {}

    get dirty(): boolean { return this.revision !== this.savedRevision; }
    get hasFile(): boolean { return this.file !== null; }

    changed(): void
    {
        this.revision++;
        this.error = null;
        this.cancelTimer();
        if(this.file !== null)
        {
            this.timer = setTimeout(() => {
                this.timer = null;
                void this.flush().catch(() => { /* The status retains the error for explicit retry. */ });
            }, this.delayMs);
        }
        this.publish();
    }

    /// Call only after the old document has been saved.
    attach(file: string | null, saved: boolean): void
    {
        if(this.running !== null) { throw new Error("Cannot switch projects during a save"); }
        this.cancelTimer();
        this.file = file;
        this.revision = 0;
        this.savedRevision = saved ? 0 : -1;
        this.error = null;
        this.publish();
    }

    async flush(): Promise<void>
    {
        this.cancelTimer();
        if(this.running !== null) { return await this.running; }
        if(!this.dirty) { return; }
        if(this.file === null) { throw new Error("Open a project first"); }
        const file = this.file;
        // Start in a microtask so running is assigned before status is published.
        const work = Promise.resolve().then(async () => {
            this.error = null;
            this.publish();
            while(this.dirty)
            {
                const revision = this.revision;
                const snapshot = cloneDocument(this.current());
                await this.write(file, snapshot);
                this.savedRevision = revision;
            }
        });
        this.running = work;
        try { await work; }
        catch(error)
        {
            this.error = error instanceof Error ? error.message : "Save failed";
            throw error;
        }
        finally { this.running = null; this.publish(); }
    }

    dispose(): void { this.cancelTimer(); }

    private async write(url: string, project: ProjectDocument): Promise<void>
    {
        const response = await fetch(url, { method: "PUT", body: await this.encode(project),
            headers: { "Content-Type": "application/json" } });
        if(!response.ok) { throw new Error(`Save failed (HTTP ${response.status})`); }
    }

    private cancelTimer(): void
    {
        if(this.timer !== null) { clearTimeout(this.timer); this.timer = null; }
    }

    private publish(): void
    {
        this.notify({ filename: this.file, dirty: this.dirty,
            saving: this.running !== null, error: this.error });
    }
}

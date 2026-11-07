import { strict as assert } from "node:assert";
import { createDocument, type ImageAsset } from "./document.ts";
import { ProjectSession, type SaveStatus } from "./project-session.ts";

function fixture()
{
    const image = (kind: "tile" | "mask"): ImageAsset => ({ id: `assets/${kind === "tile" ? "tiles" : "masks"}/${kind}.png`, kind, label: `${kind}.png`,
        blob: new Blob([kind]), dimension: 256, pixels: new Uint8ClampedArray(0),
        coverage: new Uint8Array(0), url: `blob:${kind}` });
    return createDocument([image("tile")], [image("mask")]);
}

const target = "http://localhost/projects/test/level.json";
function memoryServer()
{
    const original = globalThis.fetch;
    const state = { contents: [] as string[], fail: false, restore: () => { globalThis.fetch = original; } };
    globalThis.fetch = async (input, init) => {
        assert.equal(input, target);
        assert.equal(init?.method, "PUT");
        if(state.fail) { return new Response("", { status: 500 }); }
        state.contents.push(await (init!.body as Blob).text());
        return new Response(null, { status: 204 });
    };
    return state;
}

Deno.test("save queue serializes snapshots and follows edits made during encoding", async () => {
    const project = fixture();
    const file = memoryServer();
    let release!: () => void;
    let started!: () => void;
    const start = new Promise<void>((resolve) => { started = resolve; });
    const gate = new Promise<void>((resolve) => { release = resolve; });
    let encodes = 0;
    const statuses: SaveStatus[] = [];
    const session = new ProjectSession(() => project, async (snapshot) => {
        if(encodes++ === 0) { started(); await gate; }
        return new Blob([String(snapshot.layers[1].painted[0])]);
    }, (status) => statuses.push(status), 60_000);
    session.attach(target, true);
    project.layers[1].painted[0] = true;
    session.changed();
    const saving = session.flush();
    await start;
    project.layers[1].painted[0] = false;
    session.changed();
    const concurrent = session.flush();
    assert.throws(() => session.attach(target, true), /during a save/);
    release();
    await Promise.all([saving, concurrent]);
    assert.deepEqual(file.contents, ["true", "false"]);
    assert.equal(session.dirty, false);
    assert.equal(statuses.at(-1)!.saving, false);
    session.dispose();
    file.restore();
});

Deno.test("HTTP failures preserve dirty state and target and allow retry", async () => {
    const project = fixture();
    const file = memoryServer();
    const statuses: SaveStatus[] = [];
    const session = new ProjectSession(() => project, () => Promise.resolve(new Blob(["json"])),
        (status) => statuses.push(status), 60_000);
    session.attach(target, true);
    file.fail = true;
    session.changed();
    await assert.rejects(() => session.flush(), /HTTP 500/);
    assert.equal(session.dirty, true);
    assert.equal(file.contents.length, 0);
    assert.equal(statuses.at(-1)!.error, "Save failed (HTTP 500)");
    file.fail = false;
    await session.flush();
    assert.equal(session.dirty, false);
    session.dispose();
    file.restore();
});

Deno.test("documents require a project target and explicit saves cancel pending timers", async () => {
    const project = fixture();
    const file = memoryServer();
    const session = new ProjectSession(() => project, () => Promise.resolve(new Blob(["json"])), () => {}, 60_000);
    await assert.rejects(() => session.flush(), /Open a project/);
    session.attach(target, false);
    await session.flush();
    assert.equal(session.hasFile, true);
    assert.equal(session.dirty, false);
    session.changed();
    await session.flush();
    assert.equal(file.contents.length, 2);
    session.dispose();
    file.restore();
});

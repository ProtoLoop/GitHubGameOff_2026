import { documentAssets } from "./document.ts";
import { strict as assert } from "node:assert";
import { directoryLinks, listDirectory, listPropDirectory, listAssetTree, loadProject } from "./project-http.ts";

Deno.test("directory discovery accepts immediate children and encoded filenames only", () => {
    const base = "http://localhost/projects/";
    assert.deepEqual(directoryLinks(base, ["../", "one/", "one/", "two%20words/", "one/nested/",
        "https://elsewhere/", "?sort=name", "../outside/", "bad%2Fname/"], true),
        [base + "one/", base + "two%20words/"]);
    assert.deepEqual(directoryLinks(base, ["a%20%23.PNG", "b.webp", "c.jpeg", "notes.txt", "dir/", "../x.png"], false),
        [base + "a%20%23.PNG", base + "b.webp", base + "c.jpeg"]);
});

Deno.test("the optional prop directory ignores only 404 responses", async () => {
    const original = globalThis.fetch;
    try
    {
        globalThis.fetch = () => Promise.resolve(new Response(null, { status: 404 }));
        assert.deepEqual(await listPropDirectory("http://localhost/assets/props/"), []);
        await assert.rejects(() => listDirectory("http://localhost/assets/tiles/", false));
        for(const status of [403, 500])
        {
            globalThis.fetch = () => Promise.resolve(new Response(null, { status }));
            await assert.rejects(() => listPropDirectory("http://localhost/assets/props/"));
        }
    }
    finally { globalThis.fetch = original; }
});

/// Supply parsed hrefs directly: Deno has no browser DOMParser. HTTP and traversal stay real.
function directoryServer(listings: Record<string, string[] | number>)
{
    const originalFetch = globalThis.fetch;
    const originalParser = globalThis.DOMParser;
    const requests: string[] = [];
    globalThis.DOMParser = class {
        parseFromString(text: string) {
            return { querySelectorAll: () => (JSON.parse(text) as string[]).map((href) => ({ getAttribute: () => href })) };
        }
    } as unknown as typeof DOMParser;
    globalThis.fetch = (input) => {
        const url = String(input);
        requests.push(url);
        const listing = listings[url];
        return Promise.resolve(typeof listing === "number" || listing === undefined
            ? new Response(null, { status: listing ?? 404 }) : new Response(JSON.stringify(listing)));
    };
    return { requests, restore: () => { globalThis.fetch = originalFetch; globalThis.DOMParser = originalParser; } };
}

Deno.test("asset trees traverse nested listings once and sort full URLs while masks remain flat", async () => {
    const root = "http://localhost/projects/test/assets/tiles/";
    const server = directoryServer({
        [root]: ["z.png", "woods/", "woods/", "a/", "../", "https://elsewhere/tree/", "woods/hidden.png"],
        [root + "woods/"]: ["../", "stone.png", "deep%20%23/", "bad%2Fname/"],
        [root + "woods/deep%20%23/"]: ["100%25.png", "stone.png", "../../", "notes.txt"],
        [root + "a/"]: ["stone.png"]
    });
    try
    {
        assert.deepEqual(await listAssetTree(root, false), [
            "a/stone.png", "woods/deep%20%23/100%25.png", "woods/deep%20%23/stone.png", "woods/stone.png", "z.png"
        ].map((path) => root + path));
        assert.equal(server.requests.length, 4);
        assert.equal(new Set(server.requests).size, 4);
        server.requests.length = 0;
        assert.deepEqual(await listDirectory(root, false), [root + "z.png"]);
        assert.deepEqual(server.requests, [root]);
    }
    finally { server.restore(); }
});

Deno.test("missing or unreadable discovered subfolders fail even for optional props", async () => {
    const root = "http://localhost/projects/test/assets/props/";
    for(const status of [404, 403, 500])
    {
        const server = directoryServer({ [root]: ["nested/"], [root + "nested/"]: status });
        try { await assert.rejects(() => listPropDirectory(root), new RegExp(`HTTP ${status}`)); }
        finally { server.restore(); }
    }
});

Deno.test("project loading retains nested identities and labels, decoding filenames once", async () => {
    const folder = "http://localhost/projects/test/";
    const server = directoryServer({
        [folder + "assets/tiles/"]: ["one/", "two/"],
        [folder + "assets/tiles/one/"]: ["ground.png"],
        [folder + "assets/tiles/two/"]: ["ground.png"],
        [folder + "assets/masks/"]: ["mask.png"],
        [folder + "assets/props/"]: ["grave%20%23/", "other/"],
        [folder + "assets/props/grave%20%23/"]: ["stone%2520.png"],
        [folder + "assets/props/other/"]: ["other%2520.png"],
        [folder + "assets/tiles/one/ground.png"]: 200,
        [folder + "assets/tiles/two/ground.png"]: 200,
        [folder + "assets/masks/mask.png"]: 200,
        [folder + "assets/props/grave%20%23/stone%2520.png"]: 200,
        [folder + "assets/props/other/other%2520.png"]: 200
    });
    const originalBitmap = globalThis.createImageBitmap;
    const originalCanvas = globalThis.OffscreenCanvas;
    globalThis.createImageBitmap = (() => Promise.resolve({ width: 256, height: 256, close: () => {} })) as typeof createImageBitmap;
    globalThis.OffscreenCanvas = class {
        getContext() { return { drawImage: () => {}, getImageData: () => ({ data: new Uint8ClampedArray(256 * 256 * 4) }) }; }
    } as unknown as typeof OffscreenCanvas;
    try
    {
        const { project, saved } = await loadProject(folder);
        try
        {
            assert.equal(saved, false);
            assert.deepEqual(project.tileSheets.map((asset) => asset.id), ["assets/tiles/one/ground.png", "assets/tiles/two/ground.png"]);
            assert.deepEqual(project.sequences.map((asset) => asset.id), ["assets/props/grave #/stone%20.png", "assets/props/other/other%20.png"]);
            assert.deepEqual(project.tileSheets.map((asset) => asset.label), ["one/ground.png", "two/ground.png"]);
            assert.equal(project.sequences[0].label, "grave #/stone%20.png");
            assert.deepEqual(project.sprites.map((sprite) => sprite.name), ["stone%20", "other%20"]);
            assert.equal(project.sprites[0].sequences[0], project.sequences[0]);
        }
        finally { for(const asset of documentAssets(project)) { URL.revokeObjectURL(asset.url); } }
    }
    finally
    {
        globalThis.createImageBitmap = originalBitmap;
        globalThis.OffscreenCanvas = originalCanvas;
        server.restore();
    }
});

Deno.test("explicit sequence textures load once within the project root and failures release candidates", async () => {
    const folder = "http://localhost/projects/test/";
    const texture = folder + "assets/anims/actor%20%23.png";
    const data = {
        format: "mosaic", version: 27, gridSize: 2,
        tileSheets: ["/assets/tiles/tile.png"], maskSheets: ["/assets/masks/mask.png"], maskMode: "random",
        layers: [{ sheet: 0, cells: [] }], props: [], actors: [], propTypes: [], actorTypes: [],
        sprites: [{ name: "Actor", sequences: ["Walk", "Idle"] }],
        sequences: [
            { name: "Walk", texPath: "/assets/anims/actor #.png", size: { w: 16, h: 16 } },
            { name: "Idle", texPath: "/assets/anims/actor #.png", size: { w: 32, h: 32 } }
        ],
        editor: { activeLayer: 0, view: { zoom: 1, scrollLeft: 0, scrollTop: 0, showGrid: true } }
    };
    const server = directoryServer({
        [folder + "assets/tiles/"]: ["tile.png"], [folder + "assets/masks/"]: ["mask.png"],
        [folder + "assets/tiles/tile.png"]: 200, [folder + "assets/masks/mask.png"]: 200, [texture]: 200
    });
    const directoryFetch = globalThis.fetch;
    const originalBitmap = globalThis.createImageBitmap;
    const originalCanvas = globalThis.OffscreenCanvas;
    const originalRevoke = URL.revokeObjectURL;
    const revoked: string[] = [];
    let missing = false;
    globalThis.fetch = (input, init) => {
        if(String(input) === folder + "level.json") { return Promise.resolve(new Response(JSON.stringify(data))); }
        if(missing && String(input) === texture) { return Promise.resolve(new Response(null, { status: 404 })); }
        return directoryFetch(input, init);
    };
    globalThis.createImageBitmap = (() => Promise.resolve({ width: 256, height: 256, close: () => {} })) as typeof createImageBitmap;
    globalThis.OffscreenCanvas = class {
        getContext() { return { drawImage: () => {}, getImageData: () => ({ data: new Uint8ClampedArray(256 * 256 * 4) }) }; }
    } as unknown as typeof OffscreenCanvas;
    URL.revokeObjectURL = (url) => { revoked.push(url); originalRevoke(url); };
    try
    {
        const { project, saved } = await loadProject(folder);
        assert(saved);
        assert.equal(project.sequences.length, 2);
        assert.equal(project.sequences[0].image, project.sequences[1].image);
        assert.equal(project.sequences[0].url, project.sequences[1].url);
        assert.equal(project.sprites.length, 1);
        assert.equal(project.sprites[0].name, "Actor");
        assert.deepEqual(project.sprites[0].sequences, project.sequences);
        assert.equal(server.requests.filter((url) => url === texture).length, 1);
        for(const url of new Set(documentAssets(project).map((asset) => asset.url))) { URL.revokeObjectURL(url); }
        revoked.length = 0;
        missing = true;
        await assert.rejects(() => loadProject(folder), /HTTP 404/);
        assert.equal(new Set(revoked).size, 2);
        data.sequences[0].texPath = "/assets/../outside.png";
        await assert.rejects(() => loadProject(folder), /Invalid project asset path/);
        assert(!server.requests.some((url) => url.endsWith("outside.png")));
    }
    finally
    {
        globalThis.createImageBitmap = originalBitmap;
        globalThis.OffscreenCanvas = originalCanvas;
        URL.revokeObjectURL = originalRevoke;
        server.restore();
    }
});

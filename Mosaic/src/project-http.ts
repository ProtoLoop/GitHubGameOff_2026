import { createDocument, decodeAsset, decodePropAsset, validateDocument, type ImageAsset, type PropAsset } from "./document.ts";
import { readProject } from "./project-file.ts";

/// Accept only immediate children of the requested directory, regardless of link text.
export function directoryLinks(base: string, hrefs: readonly string[], directories: boolean): string[]
{
    const parent = new URL(base);
    const links = new Set<string>();
    for(const href of hrefs)
    {
        const url = new URL(href, parent);
        if(url.origin !== parent.origin || url.search || url.hash || !url.pathname.startsWith(parent.pathname)) { continue; }
        const suffix = url.pathname.slice(parent.pathname.length);
        const name = decodeURIComponent(directories ? suffix.replace(/\/$/, "") : suffix);
        if(!name || name === "." || name === ".." || /[/\\]/.test(name)
            || url.pathname.endsWith("/") !== directories) { continue; }
        if(directories || /\.(png|jpe?g|webp)$/i.test(name)) { links.add(url.href); }
    }
    return [...links].sort();
}

export async function listDirectory(url: string, directories: boolean): Promise<string[]>
{
    return directoryLinks(url, await readDirectoryLinks(url, false), directories);
}

/// Walk immediate-child listings once each, then sort the complete set of image URLs.
/// Only a missing optional root is ignored. Discovered subdirectories must be readable.
export async function listAssetTree(root: string, optional: boolean): Promise<string[]>
{
    const pending = [root];
    const visited = new Set<string>();
    const images = new Set<string>();
    while(pending.length > 0)
    {
        const url = pending.pop()!;
        if(visited.has(url)) { continue; }
        visited.add(url);
        const links = await readDirectoryLinks(url, optional && url === root);
        for(const image of directoryLinks(url, links, false)) { images.add(image); }
        pending.push(...directoryLinks(url, links, true));
    }
    return [...images].sort();
}

export function listPropDirectory(url: string): Promise<string[]>
{
    return listAssetTree(url, true);
}

async function readDirectoryLinks(url: string, allowMissing: boolean): Promise<string[]>
{
    const response = await fetch(url, { cache: "no-store" });
    if(allowMissing && response.status === 404) { return []; }
    if(!response.ok) { throw new Error(`Could not list ${url} (HTTP ${response.status})`); }
    const html = new DOMParser().parseFromString(await response.text(), "text/html");
    return [...html.querySelectorAll("a[href]")].map((link) => link.getAttribute("href")!);
}

/// Decode each filename component once, preserving the directory structure in asset IDs.
function relativeAssetPath(root: string, url: string): string
{
    return new URL(url).pathname.slice(new URL(root).pathname.length).split("/").map(decodeURIComponent).join("/");
}

/// The candidate owns its decoded images until installed. Failures release the entire candidate.
export async function loadProject(folder: string)
{
    const decoded: (ImageAsset | PropAsset)[] = [];
    try
    {
        for(const kind of ["tile", "mask"] as const)
        {
            const subfolder = `assets/${kind === "tile" ? "tiles" : "masks"}/`;
            const root = new URL(subfolder, folder).href;
            const urls = kind === "tile" ? await listAssetTree(root, false) : await listDirectory(root, false);
            for(const url of urls)
            {
                const filename = relativeAssetPath(root, url);
                const response = await fetch(url, { cache: "no-store" });
                if(!response.ok) { throw new Error(`Could not load ${filename} (HTTP ${response.status})`); }
                decoded.push(await decodeAsset(await response.blob(), filename, subfolder + filename, kind));
            }
        }
        const propFolder = "assets/props/";
        const propRoot = new URL(propFolder, folder).href;
        for(const url of await listPropDirectory(propRoot))
        {
            const filename = relativeAssetPath(propRoot, url);
            const response = await fetch(url, { cache: "no-store" });
            if(!response.ok) { throw new Error(`Could not load ${filename} (HTTP ${response.status})`); }
            decoded.push(await decodePropAsset(await response.blob(), filename, propFolder + filename));
        }
        const tiles = decoded.filter((asset): asset is ImageAsset => asset.kind === "tile");
        const masks = decoded.filter((asset): asset is ImageAsset => asset.kind === "mask");
        if(!tiles.length || !masks.length) { throw new Error("Project needs at least one tile sheet and one alpha mask"); }
        const response = await fetch(new URL("level.json", folder), { cache: "no-store" });
        if(!response.ok && response.status !== 404) { throw new Error(`Could not load level.json (HTTP ${response.status})`); }
        const project = response.status === 404 ? createDocument(tiles, masks)
            : await readProject(await response.blob(), decoded);
        if(response.status === 404) { project.propAssets = decoded.filter((asset) => asset.kind === "prop"); }
        validateDocument(project);
        return { project, saved: response.status !== 404 };
    }
    catch(error)
    {
        for(const asset of decoded) { URL.revokeObjectURL(asset.url); }
        throw error;
    }
}

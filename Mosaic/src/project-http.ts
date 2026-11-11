import { createDocument, decodeAsset, type ImageAsset } from "./document.ts";
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
    const response = await fetch(url, { cache: "no-store" });
    if(!response.ok) { throw new Error(`Could not list ${url} (HTTP ${response.status})`); }
    const html = new DOMParser().parseFromString(await response.text(), "text/html");
    return directoryLinks(url, [...html.querySelectorAll("a[href]")].map((link) => link.getAttribute("href")!), directories);
}

/// The candidate owns its decoded images until installed. Failures release the entire candidate.
export async function loadProject(folder: string)
{
    const decoded: ImageAsset[] = [];
    try
    {
        for(const kind of ["tile", "mask"] as const)
        {
            const subfolder = `assets/${kind === "tile" ? "tiles" : "masks"}/`;
            const urls = await listDirectory(new URL(subfolder, folder).href, false);
            for(const url of urls)
            {
                const filename = decodeURIComponent(new URL(url).pathname.split("/").at(-1)!);
                const response = await fetch(url, { cache: "no-store" });
                if(!response.ok) { throw new Error(`Could not load ${filename} (HTTP ${response.status})`); }
                decoded.push(await decodeAsset(await response.blob(), filename, subfolder + filename, kind));
            }
        }
        const tiles = decoded.filter((asset) => asset.kind === "tile");
        const masks = decoded.filter((asset) => asset.kind === "mask");
        if(!tiles.length || !masks.length) { throw new Error("Project needs at least one tile sheet and one alpha mask"); }
        const response = await fetch(new URL("level.json", folder), { cache: "no-store" });
        if(!response.ok && response.status !== 404) { throw new Error(`Could not load level.json (HTTP ${response.status})`); }
        const project = response.status === 404 ? createDocument(tiles, masks)
            : await readProject(await response.blob(), decoded);
        return { project, saved: response.status !== 404 };
    }
    catch(error)
    {
        for(const asset of decoded) { URL.revokeObjectURL(asset.url); }
        throw error;
    }
}

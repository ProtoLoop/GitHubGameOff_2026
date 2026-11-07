import { strict as assert } from "node:assert";
import { directoryLinks } from "./project-http.ts";

Deno.test("directory discovery accepts immediate children and encoded filenames only", () => {
    const base = "http://localhost/projects/";
    assert.deepEqual(directoryLinks(base, ["../", "one/", "one/", "two%20words/", "one/nested/",
        "https://elsewhere/", "?sort=name", "../outside/", "bad%2Fname/"], true),
        [base + "one/", base + "two%20words/"]);
    assert.deepEqual(directoryLinks(base, ["a%20%23.PNG", "b.webp", "c.jpeg", "notes.txt", "dir/", "../x.png"], false),
        [base + "a%20%23.PNG", base + "b.webp", base + "c.jpeg"]);
});

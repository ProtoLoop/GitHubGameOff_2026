# Mosaic projects

Each project is an immediate subfolder of the server's projects root:

```text
project-name/
  level.json
  assets/tiles/**/*.png
  assets/masks/*.png
  assets/props/**/*.png  # optional
```

PNG, JPEG, and WebP images are discovered recursively under `assets/tiles/` and
`assets/props/`, including files directly in those roots. Masks are discovered
only directly inside `assets/masks/`. The tile tree and mask folder must each
contain at least one usable image.
Without `level.json`, Mosaic opens an empty level with an unmasked base and one
overlay. Save creates the JSON file. Images are never written by the editor.

## Level JSON

The container has `format: "mosaic"` and `version: 17`. Only version 17 is supported.
Older versions are rejected. There is no migration loader.
The editor and runtime share painted-cell occupancy and deterministic rendering
rules. No terrain or mask placements are serialized. The selection and sampling
algorithms below are part of the versioned format contract.

| Field | Meaning |
| --- | --- |
| `gridSize` | Power of two from 2 to 512 painted cells along each square map axis |
| `tileSheets` | Ordered project-root terrain-sheet paths starting with `/assets/tiles/` |
| `maskSheets` | Ordered project-root mask-sheet paths starting with `/assets/masks/` |
| `maskMode` | `"random"` or an index into `maskSheets` |
| `layers` | Bottom-to-top objects with `sheet` and `cells` |
| `propAssets` | Ordered objects with `texPath`, normalized `pivot`, positive `scale`, and collision `bounds` |
| `props` | Decorative instances in back-to-front draw order |
| `editor` | Editor settings described below |

Layer `sheet` indexes `tileSheets`. Catalog indices are zero-based. The zero-based
layer array index participates in mask selection.
Reordering or removing layers can change mask choices. Terrain sampling depends
only on world pixel coordinates and the texture dimensions.
`maskMode` belongs to runtime data because it determines mask-sheet selection.

### Painted cells

An overlay layer records only painted cells:

```json
{
  "sheet": 1,
  "cells": [66, 131]
}
```

`cell = y * gridSize + x` identifies a position in the square grid, from 0
through `gridSize * gridSize - 1`. Presence means painted. Omitted cells are unpainted. A cell may appear
only once. Entries must be integer indices. The writer orders them numerically,
but input order is irrelevant.

The first layer is an implicitly full, opaque, unmasked base. Its `cells`
array must be empty. All other layers use their recorded occupancy to select
junction masks. Unpainted cells still supply material wherever a nearby mask
reveals it. Occupancy does not clip rendering at cell boundaries.

### Terrain dimensions and selection

Every terrain texture in `tileSheets` must have the same square pixel dimensions,
including textures not currently assigned to a layer. Dimensions must be from
64 through 4096 pixels per side, in multiples of 64. Each entire texture is one
seamless repeating tile. Dimensions are derived from the images, not serialized.
There is no terrain tile-size setting or procedural terrain selection.

A cell covers 64×64 world pixels. One source pixel covers one world pixel,
without rescaling. A 256×256 texture repeats every 4×4 cells. A 1024×1024 texture
repeats every 16×16 cells, covering 16 times the area per repeat.

Every layer repeats its texture from the map origin, including in unpainted
cells. Masks determine overlay visibility. Clip rendering to map bounds.
Opposite texture edges must be seamless. Terrain textures have no flips or rotations.

### Junction mask selection

Masks use a separate hash of coordinates and a seed. Arithmetic is 32-bit, with
an unsigned final result for remainder operations:

```js
function hash(x, y, seed) {
    let value = Math.imul(x ^ seed, 0x45d9f3b);
    value = Math.imul(value ^ y, 0x45d9f3b);
    value ^= value >>> 16;
    return value >>> 0;
}
```

`layerIndex` is the layer's zero-based position in `layers`.

There are `(gridSize + 1) × (gridSize + 1)` junctions. Junction `(jx, jy)` is surrounded by NW, NE, SW,
and SE painted cells, contributing bits 0, 1, 2, and 3 respectively. Cells outside
the map are unpainted. The resulting pattern is the row-major mask tile index.
Pattern 0 supplies zero coverage without sampling an image. Pattern 15 is opaque.

For a nonzero pattern:

```text
sheet = maskMode == "random" ? hash(jx, jy, 1201 + layerIndex) % maskSheets.length : maskMode
orientation = eligible[hash(jx, jy, hash(layerIndex, pattern, 7919)) % eligible.length]
```

Build `eligible` by filtering this ordered list to orientations that preserve
all four pattern bits:

| Clockwise rotation | Horizontal flip | Vertical flip |
| --- | --- | --- |
| 0° | false | false |
| 90° | false | false |
| 180° | false | false |
| 270° | false | false |
| 0° | true | false |
| 0° | false | true |
| 90° | false | true |
| 90° | true | false |

Orientations apply source-axis flips followed by clockwise rotation. To test
pattern preservation, use the inverse sampling transform below with `L = 1`
on each output corner. Its source corner must have the same occupancy bit.
Identity always qualifies. The ordering above is part of the format contract.

### Sampling and compositing

For an integer map pixel `(x, y)` and texture dimension `size`:

```text
sourceX = x % size
sourceY = y % size
```

Read RGB from the layer's texture at `(sourceX, sourceY)`. Source material alpha
is ignored. All layers share the same dimensions and source coordinates.

For rendering with one quad per cell, equivalent normalized coordinates are:

```text
cellsPerTexture = size / 64
cellWithinTexture = (worldCellCol % cellsPerTexture, worldCellRow % cellsPerTexture)
textureUV = (cellWithinTexture + localCellUV) / cellsPerTexture
```

`localCellUV` runs from 0 to 1 across each cell. Nearest sampling at pixel centers
matches the integer lookup. Texture boundaries align with cell boundaries because
dimensions are multiples of 64. A partially visible repeat at the map edge is clipped.

Mask sheets are 4×4 atlases of 64×64 masks. Coverage comes from their red channel,
not PNG alpha. A junction mask is centered at `(jx * 64, jy * 64)` and begins
32 pixels left and above that point. The mask grid is offset by half a cell from the cell grid.

For any map pixel `(x, y)`, including pixels in unpainted cells:

```text
jx = floor((x + 32) / 64)
jy = floor((y + 32) / 64)
u = (x + 32) % 64
v = (y + 32) % 64
```

Resolve the mask at that junction. Reverse its rotation first. For `L = 63`:

| Rotation | Source x before flips | Source y before flips |
| --- | --- | --- |
| 0° | `u` | `v` |
| 90° | `v` | `L - u` |
| 180° | `L - u` | `L - v` |
| 270° | `L - v` | `u` |

Then replace source x with `L - x` for horizontal flip and source y with
`L - y` for vertical flip. Sample coverage from the selected mask tile.

Composite overlays bottom to top over the opaque base. Round each RGB channel
after each layer:
`round((lower * (255 - coverage) + material * coverage) / 255)`.
The coverage preview uses the same masks without checking whether the terrain
cell is painted. Clip only at map bounds, not painted-cell boundaries.

### Decorative props

`propAssets` and `props` are required arrays, and may be empty. For example:

```json
{
  "propAssets": [
    { "texPath": "/assets/props/headstone.png", "pivot": { "x": 0.5, "y": 0.5 }, "scale": 1, "bounds": [] }
  ],
  "props": [{ "index": 0, "pos": { "x": 1.5, "y": 2.25 }, "rot": 45 }]
}
```

Each asset entry requires `texPath`, a project-root image path under
`/assets/props/`. `pivot.x` and `pivot.y` are finite numbers between 0 and 1, inclusive,
measured from the image's upper-left `(0, 0)` to its lower-right `(1, 1)`. Each
omitted pivot component defaults to `0.5`. `scale` is a finite number greater than
zero and defaults to `1` when omitted. Explicit `null` values are invalid. Saves
write `texPath`, `pivot`, `scale`, and `bounds` explicitly. An omitted `pivot`
uses both default components. Newly discovered prop images use the same defaults.

`bounds` is an optional array of collision rectangles. Omission means `[]`, or
no collision. Each rectangle requires finite `x`, `y`, `w`, and `h`, with positive
`w` and `h`. Coordinates are normalized from the sprite's upper-left corner:
X and width use image width, Y and height use image height. Rectangles may overlap
or extend outside `[0, 1]`. For example, `{"x": 0.35, "y": 0.75, "w": 0.3, "h": 0.2}`.
All instances share their asset's immutable bounds. Convert to source pixels,
then apply the same pivot, scale, rotation, and position as the image. These are
local rectangles, not world-axis-aligned rectangles. Bounds do not affect picking,
placement limits, or resizing. The Show bounds toggle is session-only.

Each instance uses `index` to reference a zero-based entry in `propAssets`. `pos.x` and `pos.y` are required finite fractional
cell coordinates for the image pivot, between `-gridSize / 2` and `+gridSize / 2`
inclusive. The origin is at the map center, with X increasing rightward and Y
increasing upward. `(0, 0)` is the corner shared by the four central cells.
Prop coordinates are rounded to the nearest 0.001 cells during placement,
movement, loading, and saving. JSON numbers use at most three decimal places
without padded trailing zeros.
`rot` is a finite clockwise angle in degrees, at least 0 and less than 360.
For top-left, Y-down map pixels used by the editor renderer, convert with
`pixelX = (x + gridSize / 2) * 64` and `pixelY = (gridSize / 2 - y) * 64`.
For example, `(1.5, 2.25)` cells puts the pivot at `(2144, 1904)` pixels on a 64-cell map.
Both scaling and clockwise rotation operate about that fixed pivot. For an image
point `(u, v)` in source pixels, subtract `(pivot.x * width, pivot.y * height)`,
multiply by `scale`, rotate clockwise in Y-down image space, then add the pivot's
map-pixel position. There is no snapping to whole cells. At scale 1, one source
pixel covers one map pixel. Source alpha is preserved. Render all props above terrain,
in array order, using ordinary source-over alpha blending and nearest sampling.
Clip artwork at map edges, even when its pivot lies inside the map.

A prop asset is a single image, not an atlas. Rectangular images are allowed,
with each dimension from 1 to 4096 pixels. The catalog supports at most 256 prop
images and the level at most 10,000 instances. Asset dimensions come from the
images. Missing referenced images, incorrect asset kinds, invalid transforms,
and invalid catalog references fail loading before installation.

The document owns prop placements. Snapshots copy their transforms and share
immutable decoded images and asset pivot/scale settings. Placement, movement, and deletion are undoable,
with a complete drag forming one action. Escape or pointer cancellation rolls back
a drag. Terrain strokes also roll back on Escape, pointer cancellation, or lost
pointer capture. Losing window focus cancels an active edit. Panning stops and
retains the current view. Saved rotation, asset pivots, and asset scale are preserved and rendered, but have no editing controls.
Prop selection, tool mode, and palette selection are transient editor state
and are not saved or undoable. Instance IDs and the next-ID counter are session-only.
The loader reconstructs IDs in array order. Picking checks scaled, rotated image rectangles
from front to back, including transparent pixels.

This format change applies to Mosaic only. No game loader changes are included here. The paths are rooted at the project
directory, which is the game's root.

### Editor settings and editing

| Field under `editor` | Meaning |
| --- | --- |
| `activeLayer` | Zero-based index of the selected layer |
| `view` | `zoom`, `scrollLeft`, `scrollTop`, `showGrid` |

There is no editor layer list or separate runtime placement list. The document
owns occupancy. Loading and saving preserve occupancy, catalog order, layer order,
and settings. Layer IDs and the next-ID counter are session-only editor state.
On load, the editor assigns internal IDs in array order and restores selection
from `activeLayer`. They do not participate in rendering. The renderer derives choices without mutating the document. Runtime
consumers can ignore `editor` but must implement the selection rules above.

Painting sets a cell's occupancy. Erasing clears it. Painting a lower layer
also clears that cell in higher layers. This changes four adjoining junction
patterns, so both previews redraw a 3×3 neighborhood. Changing mask selection
redraws coverage with the new setting. Material changes preserve painted cells.
Undo and redo restore occupancy
and settings, reproducing the corresponding rendering choices.

Map size is document state. New projects default to 64×64 cells, and the Map
settings control accepts powers of two from 2–512 cells per side. Apply opens a warning dialog with
old and new dimensions, crop/growth behavior, and the number of props removed.
Cancel is initially focused. Only confirmation performs the resize.

Resizing keeps the map center fixed. Occupancy stays in top-left row-major
storage. Copy the centered overlap using source offset `(oldSize - overlap) / 2`
and destination offset `(newSize - overlap) / 2` on both axes. Allowed sizes are
all even, so these offsets are whole cells. Growing adds base terrain and empty
overlays equally on all four sides. Shrinking crops equally on all four sides
and removes props whose absolute X or Y exceeds `newSize / 2`. Pivots exactly
on the boundary remain. Retained prop coordinates do not change.

Texture repeats and procedural mask choices still use top-left row/column
indices. Resizing can change their appearance even where occupancy is retained,
and the confirmation dialog explains this. Terrain scale, mask size, and retained
layer IDs stay unchanged. A resize is one autosaved history action. Undo restores
size, occupancy, and removed props from the prior snapshot.

### Assets and limits

Terrain and mask catalog entries, and prop entries' `texPath` fields, require one
leading `/`: `/assets/tiles/ground/grass.png`, `/assets/props/graveyard/headstone.png`,
and `/assets/masks/alpha.png`. These paths are rooted at the project directory,
which is the game's root, not at Mosaic's web-server root. For example, Mosaic
loads `/assets/props/tree.png` in project2 through
`/projects/project2/assets/props/tree.png`.

Mosaic removes the leading slash when matching serialized paths to discovered
assets. In-memory asset IDs remain project-relative, and HTTP discovery still
runs within the opened project folder. Paths contain actual filenames, not URL
escapes. Every path must start with its asset kind's root. Tile and prop paths may include subfolders, while mask paths must be flat.
After the required leading slash, empty path segments, `.` and `..` segments,
backslashes, and control characters are invalid. Paths are limited to 1024
characters excluding that leading slash. Literal percent signs are
allowed and encoded when requesting URLs. Labels use paths relative to the asset
root, so identical filenames in different subfolders remain distinguishable. Image
bytes remain in separate files. Renaming a referenced image requires updating its
catalog path.

At most 256 layers, 256 tile sheets, and 64 mask sheets are supported. Terrain
and mask images are square and at most 4096 pixels wide. There is no encoded image file-size
limit. Terrain textures must share the same dimensions, from 64 through 4096 pixels
per side in multiples of 64. Mask sheets must be 256×256,
independent of the terrain texture dimensions. Level JSON has no file-size cap on
read or write.

Catalog order is preserved on load. Discovered images are sorted by their full
encoded resource URLs. Newly discovered images append in that deterministic order,
without changing existing catalog indices or prop placement order. Saved prop pivots
and scales are preserved, and new prop entries default to pivot `(0.5, 0.5)` and scale `1`. Newly discovered terrain textures
must match the shared dimensions. Compatible sheets append without
changing existing indices or occupancy. Adding mask sheets changes random
selection because the sheet count participates in the algorithm. Fixed mask
selection is unaffected. Missing catalog assets, duplicate cells,
invalid references and invalid settings fail the load
before replacing the current document.

Undo history is session-only, bounded to 100 actions and an estimated 64 MiB
of map snapshots. Each snapshot owns its occupancy arrays and shares immutable
images. Saves serialize committed occupancy, prop placements, and rendering settings.

## HTTP contract

The app reads ordinary HTML directory listings with GET `/projects/` and
GET `/projects/<name>/assets/tiles/`, `assets/masks/`, and `assets/props/`.
A 404 for the optional props directory means an empty discovered prop catalog.
Other errors fail loading. Discovered subfolders must be readable, including
under the optional props root. A saved prop catalog still requires its referenced
images. Each listing accepts only immediate child links on the same origin.
The browser walks child directories for tiles and props, fetching each listing
once, and keeps mask discovery flat. It GETs images and `level.json` through their
resource URLs, encoding each path segment separately. A 404 for `level.json` means a new level.
Other failures are reported. Reload bypasses cached listings and images.

Save sends the complete JSON body with PUT `/projects/<name>/level.json` and
`Content-Type: application/json`. A successful HTTP response marks that revision
saved. The supplied server requires a nonnegative Content-Length without a size cap, restricts writes
to level files within the configured root, and replaces the file after receiving
the complete body. It does not interpret level JSON.

The client serializes writes, snapshots committed edits, and follows edits made
during an active save with another write. Autosave waits 600 ms after a change.
Opening or reloading first flushes pending saves. Failed writes leave the current
level dirty and available for retry. Concurrent editors use last-write-wins.

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

The container has `format: "mosaic"` and `version: 10`. Only version 10 is supported.
Older versions are rejected. There is no migration loader.
The editor and runtime share painted-cell occupancy and deterministic rendering
rules. No terrain or mask placements are serialized. The selection and sampling
algorithms below are part of the versioned format contract.

| Field | Meaning |
| --- | --- |
| `gridSize` | `64` painted cells along each map axis |
| `tileSheets` | Ordered project-relative terrain-sheet paths |
| `maskSheets` | Ordered project-relative mask-sheet paths |
| `maskMode` | `"random"` or an index into `maskSheets` |
| `layers` | Bottom-to-top objects with `sheet` and `cells` |
| `propAssets` | Ordered project-relative individual prop-image paths |
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

`cell = y * 64 + x` identifies a position in the 64×64 grid, from 0 through
4095. Presence means painted. Omitted cells are unpainted. A cell may appear
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

There are 65×65 junctions. Junction `(jx, jy)` is surrounded by NW, NE, SW,
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
  "propAssets": ["assets/props/headstone.png"],
  "props": [{ "index": 0, "x": 250.5, "y": 300, "rot": 45 }]
}
```

Each instance uses `index` to reference a zero-based entry in `propAssets`. `x` and `y` are finite map
pixel coordinates for the image center, between 0 and 4096 inclusive. The map
origin is at its top-left, with X increasing rightward and Y increasing downward.
Prop coordinates are rounded to the nearest 0.001 map pixels during placement,
movement, loading, and saving. JSON numbers use at most three decimal places
without padded trailing zeros.
`rot` is a finite clockwise angle in degrees, at least 0 and less than 360.
Images rotate around their centers, without scaling or snapping. One source pixel
covers one map pixel. Source alpha is preserved. Render all props above terrain,
in array order, using ordinary source-over alpha blending and nearest sampling.
Clip artwork at map edges, even when its center lies inside the map.

A prop asset is a single image, not an atlas. Rectangular images are allowed,
with each dimension from 1 to 4096 pixels. The catalog supports at most 256 prop
images and the level at most 10,000 instances. Asset dimensions come from the
images. Missing referenced images, incorrect asset kinds, invalid transforms,
and invalid catalog references fail loading before installation.

The document owns prop placements. Snapshots copy their transforms and share
immutable decoded images. Placement, movement, and deletion are undoable,
with a complete drag forming one action. Escape or pointer cancellation rolls back
a drag. Terrain strokes also roll back on Escape, pointer cancellation, or lost
pointer capture. Losing window focus cancels an active edit. Panning stops and
retains the current view. Saved rotation is preserved and rendered, but has no editing control.
Prop selection, tool mode, and palette selection are transient editor state
and are not saved or undoable. Instance IDs and the next-ID counter are session-only.
The loader reconstructs IDs in array order. Picking checks rotated image rectangles
from front to back, including transparent pixels.

The game does not yet consume these records. Its coordinate system and sprite
resources remain separate from this editor-only capability.

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

### Assets and limits

Catalog entries are paths such as `assets/tiles/ground/grass.png`,
`assets/props/graveyard/headstone.png`, and `assets/masks/alpha.png`. Paths contain
actual filenames, not URL escapes. Every path must start with its asset kind's
root. Tile and prop paths may include subfolders, while mask paths must be flat.
Empty path segments, `.` and `..` segments, backslashes, and control characters
are invalid. Paths are limited to 1024 characters. Literal percent signs are
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
without changing existing catalog indices or prop placement order. Newly discovered terrain textures
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

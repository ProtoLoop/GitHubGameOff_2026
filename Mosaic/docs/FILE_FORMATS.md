# Mosaic projects

Each project is an immediate subfolder of the server's projects root:

```text
project-name/
  level.json
  assets/tiles/*.png
  assets/masks/*.png
```

PNG, JPEG, and WebP images are discovered directly in the two asset folders.
Nested folders are ignored. Both folders must contain at least one usable image.
Without `level.json`, Mosaic opens an empty level with an unmasked base and one
overlay. Save creates the JSON file. Images are never written by the editor.

## Level JSON

The container has `format: "mosaic"` and `version: 6`. Only version 6 is supported.
The editor and runtime share painted-cell occupancy and deterministic rendering
rules. No terrain or mask placements are serialized. The selection and sampling
algorithms below are part of the versioned format contract.

| Field | Meaning |
| --- | --- |
| `gridSize` | `64` painted cells along each map axis |
| `tileSize` | Level-wide terrain tile size in source and world pixels |
| `tileSheets` | Ordered project-relative terrain-sheet paths |
| `maskSheets` | Ordered project-relative mask-sheet paths |
| `maskMode` | `"random"` or an index into `maskSheets` |
| `layers` | Bottom-to-top objects with `sheet` and `cells` |
| `editor` | Editor settings described below |

Layer `sheet` indexes `tileSheets`. Catalog indices and atlas tile indices are
zero-based. The zero-based layer array index participates in mask selection.
Reordering or removing layers can change mask choices. Terrain selection depends
only on world terrain tile coordinates and the shared atlas layout.
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

Every terrain atlas in `tileSheets` must have the same square pixel dimensions,
including atlases not currently assigned to a layer. The level has one `tileSize`,
shared by every layer. It must be a positive multiple of 64 and divide the atlas
dimension exactly. `atlasGridSize = imageWidth / tileSize` is the number of terrain
tiles along either atlas axis. Any positive integral N×N layout is valid, including
1×1. Atlas dimensions and `atlasGridSize` are derived, not serialized.

A cell covers 64×64 world pixels. A terrain tile covers
`tileCellSize = tileSize / 64` cells along each axis. One source pixel covers one
world pixel, without rescaling. For a 4096×4096 atlas:

| `tileSize` | `atlasGridSize` | Cells covered by one terrain tile |
| --- | --- | --- |
| 64 | 64 | 1×1 |
| 128 | 32 | 2×2 |
| 4096 | 1 | 64×64 |

The world terrain tile grid starts at the map origin. For a cell at
`(worldCellCol, worldCellRow)`:

```text
worldTerrainCol = floor(worldCellCol / tileCellSize)
worldTerrainRow = floor(worldCellRow / tileCellSize)
terrainColumnCount = ceil(gridSize / tileCellSize)
worldTerrainIndex = worldTerrainRow * terrainColumnCount + worldTerrainCol
```

Ceiling division includes a partially visible terrain tile at the map boundary.
Clip terrain tiles to the map bounds. The row-major index includes the map width,
so consumers must use the level's width, not the viewport width.

Terrain selection uses the shader's `lowbias32` hash. All arithmetic is unsigned
32-bit. In TypeScript, `Math.imul` multiplies modulo 2^32 and `>>>` is unsigned
right shift. Convert the final result to unsigned before taking a remainder:

```js
function terrainHash(value) {
    let h = value >>> 0;
    h = Math.imul(h ^ (h >>> 16), 0x7feb352d);
    h = Math.imul(h ^ (h >>> 15), 0x846ca68b);
    return (h ^ (h >>> 16)) >>> 0;
}
```

Select the atlas tile using the hash result:

```text
hashVal = terrainHash(worldTerrainIndex)
tileIndex = hashVal % (atlasGridSize * atlasGridSize)
```

Every cell within a world terrain tile shares its selection, including
unpainted cells. No layer identity or occupancy enters the hash.
Each layer samples its own `sheet`, with the same terrain choices at the same
world coordinates. Terrain sheets are not selected procedurally.

Reference values for a 4×4 atlas:

| `worldTerrainIndex` | Hash | `tileIndex` |
| --- | --- | --- |
| 0 | `0x00000000` | 0 |
| 1 | `0x688990c0` | 0 |
| 2 | `0xd1132181` | 1 |
| 3 | `0x53f1e9dd` | 13 |
| 64 | `0xdce420ba` | 10 |
| 65 | `0x15e7abd3` | 3 |

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

For an integer map pixel `(x, y)`, find its world terrain tile and resolve the
atlas selection as above. Find the source pixel within that tile:

```text
localX = x % tileSize
localY = y % tileSize
atlasX = (tileIndex % atlasGridSize) * tileSize + localX
atlasY = floor(tileIndex / atlasGridSize) * tileSize + localY
```

Read RGB from the layer's atlas at `(atlasX, atlasY)`. Source material alpha is
ignored.

For rendering with one quad per cell, the equivalent normalized coordinates are:

```text
cellWithinTile = (worldCellCol % tileCellSize, worldCellRow % tileCellSize)
terrainUV = (cellWithinTile + localCellUV) / tileCellSize
atlasTileOffset = (tileIndex % atlasGridSize, floor(tileIndex / atlasGridSize))
atlasUV = (terrainUV + atlasTileOffset) / atlasGridSize
```

`localCellUV` runs from 0 to 1 across each cell. Nearest sampling at pixel centers
matches the integer lookup. For a 128-pixel terrain tile, the four cells cover
the four half-width UV regions. For a 4096-pixel tile, the first pixel
of cell `(1, 0)` reads source pixel `(64, 0)`.

Adjacent terrain tiles require compatible edge artwork.

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
redraws coverage with the new setting. Tile-size or material changes recalculate
texture selection and preserve painted cells. Undo and redo restore occupancy
and settings, reproducing the corresponding rendering choices.

### Assets and limits

Catalog entries are paths such as `assets/tiles/grass.png` and
`assets/masks/alpha.png`. Paths contain actual filenames, not URL escapes.
Labels come from filenames. Image bytes remain in separate files. Renaming a
referenced image requires updating its catalog path.

At most 256 layers, 256 tile sheets, and 64 mask sheets are supported. Images
are square and at most 4096 pixels wide. There is no encoded image file-size
limit. Terrain atlases must share the same dimensions and divide into the same
N×N grid at the level-wide terrain tile size. Mask sheets must be 256×256,
independent of the terrain atlas dimensions. Level JSON has no file-size cap on
read or write.

Catalog order is preserved on load. Newly discovered terrain atlases must match
the shared dimensions and tile size. Compatible sheets append without
changing existing indices or occupancy. Adding mask sheets changes random
selection because the sheet count participates in the algorithm. Fixed mask
selection is unaffected. Missing catalog assets, duplicate cells,
invalid references and invalid settings fail the load
before replacing the current document.

Undo history is session-only, bounded to 100 actions and an estimated 64 MiB
of map snapshots. Each snapshot owns its occupancy arrays and shares immutable
images. Saves serialize the committed occupancy and rendering settings.

## HTTP contract

The app reads ordinary HTML directory listings with GET `/projects/` and
GET `/projects/<name>/assets/tiles/` or `assets/masks/`. Only immediate child
links are used. It GETs images and `level.json` through their resource URLs,
encoding each filename as a URL component. A 404 for `level.json` means a new level.
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

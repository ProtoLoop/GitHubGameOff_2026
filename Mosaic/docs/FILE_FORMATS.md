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

The container has `format: "mosaic"` and `version: 4`. Versions 1–3 are not
supported. The editor and runtime share painted-cell occupancy and deterministic
rendering rules. No terrain or mask placements are serialized. Version 4 was
revised in place to use numeric cell arrays and omit layer IDs. Earlier
version-4 layouts are not supported.

| Field | Meaning |
| --- | --- |
| `gridSize` | `64` painted cells along each map axis |
| `tileSize` | Shared source terrain tile size in pixels |
| `tileSheets` | Ordered project-relative terrain-sheet paths |
| `maskSheets` | Ordered project-relative mask-sheet paths |
| `maskMode` | `"random"` or an index into `maskSheets` |
| `layers` | Bottom-to-top objects with `sheet` and `cells` |
| `editor` | Editor settings described below |

Layer `sheet` indexes `tileSheets`. Catalog indices and atlas tile indices are
zero-based. The zero-based layer array index participates in selection.
Reordering or removing layers changes choices for layers whose indices change.
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

### Deterministic texture and mask selection

Both editor and runtime use the following rules. Hash arithmetic is 32-bit.
`imul` multiplies modulo 2^32, `^` is bitwise XOR, and `>>>` is unsigned shift.
The final unsigned result is used for every modulo operation.

```js
function hash(x, y, seed) {
    let value = Math.imul(x ^ seed, 0x45d9f3b);
    value = Math.imul(value ^ y, 0x45d9f3b);
    value ^= value >>> 16;
    return value >>> 0;
}
```

For every terrain cell `(x, y)`, including the base and unpainted overlays,
`layerIndex` is the layer's current zero-based position in `layers`:

```text
atlasGrid = imageWidth / tileSize
terrainTile = hash(x, y, layerIndex) % (atlasGrid * atlasGrid)
flip = hash(x, y, hash(layerIndex, 0, 3571)) % 4
hflip = (flip & 1) != 0
vflip = (flip & 2) != 0
```

Terrain is never rotated. These numbers describe the algorithm, not serialized
transform fields. Terrain indices and flips are independent of occupancy.

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

Terrain tiles remain on the cell grid. Tile sheets are square. Mosaic displays
cells at 64×64 pixels before zoom, applies terrain flips in that coordinate
space, and samples the selected source tile at
`floor(pixelCoordinate * tileSize / 64)`. Source material alpha is ignored.

Mask sheets are 4×4 atlases of 64×64 masks. Coverage comes from their red channel,
not PNG alpha. A junction mask is centered at `(jx * 64, jy * 64)` and begins
32 pixels left and above that point. The half-cell offset is unchanged.

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
limit. Mask sheets must be
256×256. Tile sheets used by layers must divide into an even grid of at least
2×2 tiles. Level JSON has no file-size cap on read or write.

Catalog order is preserved on load. Newly discovered sheets append without
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

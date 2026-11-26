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

The container has `format: "mosaic"` and `version: 27`. Only version 27 is supported.
Older versions are rejected. There is no migration loader.

Saves use two-space indentation, with small objects containing only scalar values
written on one line using compact JSON when they fit within 120 columns.
Nonempty arrays and larger or nested objects use multiple lines.

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
| `sequences` | Ordered named sprite sequences with `texPath`, `srcRect`, optional frame `size`, `pivot`, `scale`, `tint`, and `collisionRects` |
| `sprites` | Named sprites with ordered sequence-name references, one per facing direction |
| `propTypes` | Named prop definitions with a `sprite` name reference |
| `actorTypes` | Named actor definitions with a runtime `class` and a `sprite` name reference |
| `props` | Decorative placements referencing prop definitions through `type` |
| `actors` | Actor placements referencing actor definitions through `type` |
| `editor` | Editor settings described below |
| `extensions` | Optional opaque JSON preserved across loads and saves |

Mosaic retains `extensions` without interpreting or editing its contents. Objects,
arrays, strings, numbers, booleans, and explicit `null` are supported. When absent,
the key remains absent on save, including for new projects. History and save
snapshots share this immutable data, so edits, undo, redo, and autosave retain it.
Preservation applies to parsed JSON values, not whitespace or original formatting,
and numbers use JavaScript's numeric precision. Other unknown fields are not preserved.
Extension preservation was introduced in version 21 and continues in version 27.

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

`sequences`, `sprites`, and `props` are required arrays, and may be empty. For example:

```json
{
  "sequences": [
    { "name": "tree", "texPath": "/assets/props/tree.png", "srcRect": {}, "collisionRects": [] },
    { "name": "headstone", "texPath": "/assets/props/headstone.png", "srcRect": {}, "collisionRects": [] }
  ],
  "sprites": [
    { "name": "tree", "sequences": ["tree"] },
    { "name": "headstone", "sequences": ["headstone"] }
  ],
  "propTypes": [{ "name": "tree", "sprite": "tree" }],
  "actorTypes": [{ "name": "headstone", "class": "Tombstone", "sprite": "headstone" }],
  "props": [{ "type": "tree", "pos": { "x": 1.5, "y": 2.25 }, "rot": 45 }],
  "actors": [{ "type": "headstone", "pos": { "x": -2, "y": 1 }, "rot": 0 }]
}
```

Each sequence entry requires `texPath`, a project-root image path under
`/assets/`. Automatic discovery remains limited to `/assets/props/`. `pivot.x` and `pivot.y` are finite numbers between 0 and 1, inclusive,
measured from one frame's upper-left `(0, 0)` to its lower-right `(1, 1)`. Each
omitted pivot component defaults to `0.5`. `scale` is a finite number greater than
zero and defaults to `1` when omitted. Explicit `null` values are invalid. Saves
write `name`, `texPath`, `srcRect`, `pivot`, `scale`, `tint`, and `collisionRects` explicitly. An omitted `pivot`
uses both default components. Newly discovered prop images use the same defaults.

`srcRect` is optional. An empty object `{}` selects the full texture. A rectangle
object selects a sprite with required integer `x`, `y`, `w`,
and `h` in texture pixels, measured from the texture's upper-left corner with
X rightward and Y downward. Coordinates must be nonnegative, dimensions positive,
and the rectangle must fit entirely within the decoded texture. For example,
`{"x": 32, "y": 64, "w": 128, "h": 96}` selects a 128×96 sprite.
Omission also selects the full texture. Internally this resolves to
`{ "x": 0, "y": 0, "w": textureWidth, "h": textureHeight }`.
Explicit `null`, all arrays, nonempty objects missing any required rectangle
component, and fractional components are invalid.
Newly discovered assets also use the full texture. Saves write `{}` whenever the
rectangle covers the entire texture, including when loaded from an explicit
full-texture object. Cropped rectangles save as objects. The source rectangle
selects the region containing the sequence's frames. Drawing, palette and placement
previews, picking, selection outlines, and viewport culling all use frame zero
of the sprite's first sequence.
The source offset selects pixels without shifting the sprite's world pivot.

`name` identifies a sequence independently of its texture. Names are case-sensitive,
unique within the catalog, nonblank, and at most 63 UTF-8 bytes. ASCII control
characters are rejected. An omitted name is generated from the texture filename
without its final extension, then written explicitly on save. Duplicate generated
names or overlong names fail loading and require explicit names in JSON. Saved
names are retained independently of path changes. Multiple named sequences may
share a texture, including identical source rectangles.

`size: { w, h }` optionally gives positive integer frame dimensions, each no larger
than the resolved source rectangle. Omission means one frame covering that entire
rectangle, not the entire texture unless the rectangle selects it. Saves preserve
omission, and preserve explicit dimensions even when they cover the full rectangle.
Empty objects, partial objects, arrays, and explicit `null` are invalid.
Frames advance left to right, then top to bottom. The frame count is
`floor(srcRect.w / size.w) * floor(srcRect.h / size.h)` and incomplete edge rows or
columns are ignored. Frame zero starts at `(srcRect.x, srcRect.y)`.
Mosaic does not animate sequences. Pivots, collision rectangles, scaled bounds,
and picking are relative to one frame. Without `size`, existing static props keep
their source-rectangle geometry.

`tint: { r, g, b, a }` multiplies the source RGB and alpha channels in all sprite
previews. Every component must be a finite number in `[0, 1]`. Omission defaults
all four components to 1. Explicit tint objects require all four components.
Arrays, `null`, and incomplete objects are invalid. Saves write tint explicitly.
Tinted frame-zero images are cached per immutable catalog entry while the decoded
source texture remains shared. Selection and collision overlay colors are unaffected.

`collisionRects` is an optional array of collision rectangles. Omission means `[]`, or
no collision. Each rectangle requires finite `x`, `y`, `w`, and `h`, with positive
`w` and `h`. Coordinates are normalized relative to one frame before
scale and rotation: `(0, 0)` is its upper-left corner and `(1, 1)` its lower-right,
with X increasing rightward and Y downward. X and width use the resolved frame width,
Y and height use the resolved frame height. Rectangles may overlap
or extend outside `[0, 1]`. For example, `{"x": 0.35, "y": 0.75, "w": 0.3, "h": 0.2}`.
All instances share their asset's immutable collision rectangles. Convert to frame pixels,
then apply the same pivot, scale, rotation, and position as the image. These are
local rectangles, not world-axis-aligned rectangles. Collision rectangles do not affect picking,
placement limits, or resizing. The Show collision rectangles toggle is session-only.

Each sprite requires a `name` and a nonempty `sequences` array of sequence names.
Names follow the sequence-name rules, but sprite names are required and unique
within their own catalog. A sprite may have the same name as a sequence. Each
sequence reference identifies a facing direction, in the saved array order.
Sequences may be shared between sprites. Mosaic displays frame zero of the first
sequence, including its geometry, tint, pivot, scale, and collision rectangles.
Rotation transforms that image and does not select another facing direction.
There is no animation playback or sprite-level metadata inheritance yet.

### Prop and actor definitions and placements

The required `propTypes` and `actorTypes` arrays explicitly determine the two palettes.
Both may be empty and support at most 256 definitions each. A prop definition has
`name` and `sprite`. An actor definition additionally requires `class`, a runtime
implementation name. Mosaic stores that string without interpreting it or restricting
it to a built-in list. Spawning policies, brains, and multi-sprite actor bindings
are outside this version's editor contract.

Definition names are unique within each catalog and follow the sprite-name rules.
Actor `class` follows the same nonblank, 63 UTF-8 byte, no-control-character rules.
Several definitions may use the same runtime class. `sprite` must resolve to the
shared sprite catalog. Names need not match sprite names. Palette thumbnails and
placement previews use frame zero of the referenced sprite's first sequence.
Neither palette lists sprites automatically. Newly discovered images add sequence
and sprite resources only, and definitions are maintained in JSON and reloaded.

Both placement arrays are required, even when empty. Each `props` entry references
a prop definition with `type: "name"`. Each `actors` entry references an actor
definition with `type: "name"`. Both carry `pos` and `rot` as described below.
Missing or malformed references and duplicate definition names fail loading before
installation. A sprite name alone is no longer a valid placement reference.

Reordering the top-level definition,
sequence or sprite catalog does not change references. Reordering a sprite's
sequence list changes its direction order and which sequence Mosaic displays.

`pos.x` and `pos.y` are required finite fractional
cell coordinates for the image pivot, between `-gridSize / 2` and `+gridSize / 2`
inclusive. The origin is at the map center, with X increasing rightward and Y
increasing upward. `(0, 0)` is the corner shared by the four central cells.
Placement coordinates are rounded to the nearest 0.001 cells during placement,
movement, loading, and saving. JSON numbers use at most three decimal places
without padded trailing zeros.
`rot` is a finite clockwise angle in degrees, at least 0 and less than 360.
For top-left, Y-down map pixels used by the editor renderer, convert with
`pixelX = (x + gridSize / 2) * 64` and `pixelY = (gridSize / 2 - y) * 64`.
For example, `(1.5, 2.25)` cells puts the pivot at `(2144, 1904)` pixels on a 64-cell map.
Both scaling and clockwise rotation operate about that fixed pivot. For a point `(u, v)` in texture pixels, first subtract `(srcRect.x, srcRect.y)`,
then subtract `(pivot.x * frameWidth, pivot.y * frameHeight)`,
multiply by `scale`, rotate clockwise in Y-down image space, then add the pivot's
map-pixel position. There is no snapping to whole cells. At scale 1, one source
pixel covers one map pixel. Source alpha is preserved. Render props above terrain and actor previews above props, each in its saved
array order, using ordinary source-over alpha blending and nearest sampling.
This editor ordering does not prescribe runtime actor rendering. Actors have a
16-pixel green square marker with a dark border at their pivot, independent of
zoom and rotation. The marker also appears in the placement preview.
Clip artwork at map edges, even when its pivot lies inside the map.

A catalog entry selects a sprite sequence within its source texture. Rectangular textures are allowed,
with each dimension from 1 to 4096 pixels. The catalog supports at most 256 named
sequences, 256 sprites, and 256 sequence references per sprite. The level supports
at most 10,000 combined prop and actor instances. Asset dimensions come from the
images. Sequence entries sharing a texture share its decoded image and URL.
Missing referenced images, incorrect asset kinds, invalid source rectangles or transforms,
and invalid catalog references fail loading before installation.

The document owns prop and actor placements in one list, props followed by actors. Snapshots copy their transforms and share
immutable prop and actor definitions, sprite definitions, sequence definitions, and decoded images. Placement, movement, and deletion are undoable,
with a complete drag forming one action. Escape or pointer cancellation rolls back
a drag. Terrain strokes also roll back on Escape, pointer cancellation, or lost
pointer capture. Losing window focus cancels an active edit. Panning stops and
retains the current view. Saved rotation and asset source rectangles, pivots, and scale are preserved and rendered, but have no editing controls.
Object selection, tool mode, and palette selection are transient editor state
and are not saved or undoable. Instance IDs and the next-ID counter are session-only.
The loader reconstructs IDs in prop array order followed by actor array order. Picking checks scaled, rotated image rectangles
from front to back, including transparent pixels.

The C++ loader parses `propTypes` and `actorTypes`. Prop placements reference
`propTypes` through their `type` field, and the game resolves these definitions
to create props. Actor placements remain editor-only: the runtime does not yet
parse or instantiate them.
The paths are rooted at the project directory, which is the game's root.

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
old and new dimensions, crop/growth behavior, and the numbers of props and actors removed.
Cancel is initially focused. Only confirmation performs the resize.

Resizing keeps the map center fixed. Occupancy stays in top-left row-major
storage. Copy the centered overlap using source offset `(oldSize - overlap) / 2`
and destination offset `(newSize - overlap) / 2` on both axes. Allowed sizes are
all even, so these offsets are whole cells. Growing adds base terrain and empty
overlays equally on all four sides. Shrinking crops equally on all four sides
and removes prop and actor placements whose absolute X or Y exceeds `newSize / 2`. Pivots exactly
on the boundary remain. Retained placement coordinates do not change.

Texture repeats and procedural mask choices still use top-left row/column
indices. Resizing can change their appearance even where occupancy is retained,
and the confirmation dialog explains this. Terrain scale, mask size, and retained
layer IDs stay unchanged. A resize is one autosaved history action. Undo restores
size, occupancy, and removed props and actors from the prior snapshot.

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
without changing existing references or placement order. Saved sequence names, source rectangles, optional dimensions, tints, pivots,
and scales are preserved. Textures already referenced by any saved sequence are
not appended as new entries. Each new sequence also gets a same-named sprite
containing only that sequence. Existing sprite groups are preserved. A generated
name conflicting with either catalog fails loading rather than silently renaming
or merging definitions. New entries use the filename stem as name, the full
texture, omitted `size`, white tint, pivot `(0.5, 0.5)`, and scale `1`. Newly discovered terrain textures
must match the shared dimensions. Compatible sheets append without
changing existing indices or occupancy. Adding mask sheets changes random
selection because the sheet count participates in the algorithm. Fixed mask
selection is unaffected. Missing catalog assets, duplicate cells,
invalid references and invalid settings fail the load
before replacing the current document.

Undo history is session-only, bounded to 100 actions and an estimated 64 MiB
of map snapshots. Each snapshot owns its occupancy arrays and shares immutable
images. Saves serialize committed occupancy, prop and actor definitions and placements, and rendering settings.

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
After reading `level.json`, the client fetches additional explicitly referenced
sequence textures anywhere under that project's `assets/` directory. Paths retain
literal filename characters and are encoded by segment for HTTP requests.
Traversal and paths outside `assets/` are rejected. Each distinct sequence texture
is decoded once per project load, including when multiple entries reference it.
Missing referenced textures fail loading before installation. Project2 includes
its referenced actor textures. No repository-level fallback or placeholder images
are used.

Save sends the complete JSON body with PUT `/projects/<name>/level.json` and
`Content-Type: application/json`. A successful HTTP response marks that revision
saved. The supplied server requires a nonnegative Content-Length without a size cap, restricts writes
to level files within the configured root, and replaces the file after receiving
the complete body. It does not interpret level JSON.

The client serializes writes, snapshots committed edits, and follows edits made
during an active save with another write. Autosave waits 600 ms after a change.
Opening or reloading first flushes pending saves. Failed writes leave the current
level dirty and available for retry. Concurrent editors use last-write-wins.

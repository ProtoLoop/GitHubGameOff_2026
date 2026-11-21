# Architecture Decisions

This document records important architectural choices when the reasons behind
them will be useful later.

Add a decision when it affects several parts of the system, limits future
choices, or is likely to be questioned again after the original context has
been forgotten. Routine implementation choices belong in the code or in a
pattern.

Each decision should include:

- a title, date, and status.
- the situation that required a choice.
- what we decided.
- the alternatives we considered.
- the good and bad consequences.
- links to related principles, patterns, and older decisions.

An accepted decision is part of the project's history. If the decision changes,
add a new entry and mark the old one as superseded instead of rewriting it.

## Bootstrap platform and build foundation

Date: 2026-11-02. Status: Accepted.

The game is primarily browser-based C++, with native development on macOS and
future Windows/Linux support. It needs a small build that produces consistent
SDL behavior across toolchains without bringing in an engine framework.

We use CMake with Ninja Multi-Config and separate platform build trees. Debug
and Release share a platform tree. Commands are documented explicitly rather
than stored in presets, matching the project's existing workflow. Separate
configuration trees were considered, but would duplicate platform setup.

SDL supplies platform services and the initial 2D renderer. It is built statically
from the official Git submodule, pinned to release 3.4.18. This makes dependency
selection explicit and avoids requiring a matching system SDL installation or
shipping a separate SDL dynamic library. It adds submodule initialization and
dependency compilation to the development workflow. Dependency updates require
an intentional Git pointer change and a notice review.

The application uses SDL callbacks instead of a blocking main loop. SDL schedules
the same lifecycle on native and browser targets, satisfying the browser's need
to regain control between frames. A platform-specific Emscripten loop was an
alternative, but would duplicate lifecycle wiring in the bootstrap. Browser
threading is deferred until there is a concrete need.

These choices follow [thin abstractions and clear ownership](principles.md).
The current structure and limitations are described in [development guide](../development.md#build-structure).

## Project-local web toolchain

Date: 2026-11-02. Status: Accepted.

Web builds need a consistent compiler without requiring each developer to
install and activate Emscripten or supply machine-specific SDK paths. Package
manager layouts differ, and the Emscripten source repository alone does not
include the LLVM, Binaryen, and Node executables needed to build.

The root `Emscripten.cmake` provisions pinned official Emscripten and Node archives
on the first web configure. Versions, host mappings, and hashes are checked in.
Downloaded tools are cached outside version control. Offline machines can use
archives supplied ahead of time, with downloads explicitly disabled.

We considered relying on system/Nix Emscripten, adding the emsdk manager as a
submodule, and vendoring Emscripten source. Automatic archive provisioning meets
the desired configure-and-build workflow without a separate SDK-manager step.
It adds download/extraction logic to maintain and requires substantial local
storage. Python, CMake, Ninja, and clang-tidy remain host prerequisites.

This extends the [bootstrap foundation](#bootstrap-platform-and-build-foundation)
with an explicit toolchain owner, following [clear responsibility](principles.md).
The layout and offline behavior are described in [development guide](../development.md#web-compiler-provisioning).

## Mosaic project folders and HTTP persistence

Date: 2026-11-06. Status: Accepted.

Mosaic projects are user-owned folders containing `level.json`, `assets/tiles/`,
and `assets/masks/`. The browser discovers projects and images by reading links
from ordinary HTTP directory listings. Images are loaded at runtime, so changing
assets requires reloading the project without rebuilding the app. Filenames
supply accessible labels. Level JSON references project-relative image paths.
Stable layer IDs, stored tile indices, and saved mask order preserve rendering.
Newly discovered masks append to that order. Missing referenced images fail the
load rather than silently changing materials.

A small Python static-file server maps `/projects/` to a configured directory
and adds PUT for project `level.json` resources. It writes through a temporary
file and replaces the target after receiving the complete body. It has no
application-specific listing endpoints, database, or map-format knowledge.
Directory listing support and writable level resources are server requirements.
Concurrent editors use last-write-wins behavior.

The document owns map state and shares immutable decoded images with bounded
undo snapshots. A complete stroke is one undo action. Selection and view settings
are saved but excluded from undo history. Opening or reloading clears history.
The session serializes HTTP saves and marks a revision saved only after a
successful response. Completed edits autosave after a brief pause. Switching or
reloading first flushes pending saves, and failure preserves the current project.
The candidate document is validated before installation. Images remain alive
while referenced by the current catalog, document, or history.

The server stays independent of editor semantics. JSON parsing and validation,
asset discovery, history, and save scheduling remain in the browser. This follows
[clear ownership](principles.md#make-it-clear-who-is-responsible). See the
[file formats](../../Mosaic/docs/FILE_FORMATS.md) and
[user guide](../../Mosaic/README.md).


## Mosaic resolved runtime data and editor state

Date: 2026-11-06. Status: Superseded by [shared sparse placements](#mosaic-shared-sparse-placements).

The game needs to render Mosaic levels without reproducing the editor's tile
selection, mask placement, or orientation algorithms. Occupancy alone is not
sufficient because junction masks also cover portions of empty cells.

Version 2 of `level.json` separates `runtime` data from `editor` state. Every
runtime cell stores its resolved material index. Overlay cells also
store four resolved junction mask descriptors. Stable IDs, occupancy, selection,
and view settings belong to the editor section. Both Mosaic previews consume the
runtime representation after placement has been resolved. Saving uses the same
placement code on the committed document snapshot.

We considered continuing to derive placement in every consumer. Persisting the
results gives runtime consumers an explicit rendering contract and keeps editor
algorithms out of the game. It increases file size and requires consistency
validation between the two sections when Mosaic opens a file. Version 1 is
replaced without a compatibility loader. This extends the
[project persistence decision](#mosaic-project-folders-and-http-persistence).

The boundary follows [clear responsibility](principles.md#make-it-clear-who-is-responsible).
The two formats and sampling conventions are specified in
[File formats](../../Mosaic/docs/FILE_FORMATS.md).


## Mosaic shared sparse placements

Date: 2026-11-06. Status: Superseded by [Mosaic painted occupancy and procedural rendering](#mosaic-painted-occupancy-and-procedural-rendering).

The version 2 format duplicated layer structure between runtime and editor
sections and stored a resolved terrain tile for every cell. That obscured the
intended cell model: a terrain cell contains a placed tile or is empty. Numeric
transform codes and repeated per-cell junction descriptors also made the file
hard to read.

Version 3 uses one shared layer list. Each layer has sparse terrain placements
on the 64×64 cell grid and sparse mask placements on the 65×65 junction grid.
Top-level `tileSheets` and `maskSheets` catalogs supply stable sheet indices.
Tile and mask properties have names. Mask flips are booleans and clockwise rotations
are the strings `"0"`, `"90"`, `"180"`, and `"270"`, applied after source-axis
flips. Only view, selection, and placement settings remain under `editor`.

The document owns these placements, and loading, saving, rendering, and history
preserve them. Placement algorithms run when editing requires new choices.
Empty terrain cells contribute no material, even under a neighboring mask.
Missing junctions supply zero coverage. The base remains fully occupied and
unmasked. Both previews follow the same rules as runtime consumers.

We considered retaining parallel editor occupancy or reconstructing saved
orientations on load. Shared authoritative placements avoid both duplication
and dependence on placement algorithms during loading. Sparse lists avoid empty
records and repeated junction data. Named fields and indented JSON cost more
bytes than packed numeric records. Internally, dense lookup arrays support cell
access and undo snapshots without changing the sparse file contract.

Version 3 replaces version 2 without a compatibility loader. The sample is
converted. This supersedes the
[version 2 representation](#mosaic-resolved-runtime-data-and-editor-state), while
preserving the [project persistence lifecycle](#mosaic-project-folders-and-http-persistence).
It follows [clear responsibility](principles.md#make-it-clear-who-is-responsible).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md).


## Mosaic painted occupancy and procedural rendering

Date: 2026-11-06. Status: Accepted for version 4. Layer identity and versioning are amended by
[Mosaic layer order as rendering identity](#mosaic-layer-order-as-rendering-identity).

Version 3 treated unpainted cells as absent material. Junction masks extend into
those cells, so skipping their material clipped soft coverage at cell boundaries.
Diagonally touching painted cells exposed this as hard straight edges.

Version 4 restores continuous material under mask-controlled visibility. Terrain
textures remain on the cell grid and masks remain offset by half a cell. Layers
save sparse painted-cell records rather than resolved terrain or mask placements.
The first layer is implicitly full and unmasked. Its serialized cell list is empty.
Both editor and runtime deterministically select terrain tiles, junction
mask patterns, mask sheets, and orientations from occupancy, coordinates, stable
layer IDs, and rendering settings. Mask selection is top-level runtime data.
No separate editor occupancy or runtime placement lists are needed.

The document owns painted flags. Rendering derives placements without mutating
that state. History and saves preserve occupancy and settings. Texture or tile-size
changes do not remove painted cells. Runtime consumers now implement the exact
selection algorithm, which is part of the versioned format contract. Changing
that algorithm requires a format-version change. Adding sheets to the mask catalog
changes random mask selection, while fixed sheet selection remains stable.

We considered saving resolved material in every cell revealed by a mask and
retaining masks as the authoritative data. Painted occupancy is simpler and keeps
the file sparse, at the cost of requiring shared selection rules in consumers.
We also considered moving masks onto cells. That requires different neighborhood
semantics and artwork, so the existing 16-pattern junction atlas is retained.

Version 4 replaces version 3 without a compatibility loader. The current sample
is converted using its painted terrain cells. This supersedes
[Mosaic shared sparse placements](#mosaic-shared-sparse-placements).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md) and
[Coverage masks](../COVERAGE_MASKS.md).


## Mosaic layer order as rendering identity

Date: 2026-11-06. Status: Accepted for version 4.

Layer objects serialize only their material-sheet index and numeric painted-cell
array. Persistent layer IDs are unnecessary when layer order already identifies
each layer. The editor saves selection as `editor.activeLayer`, a zero-based
array index, and omits `nextLayerId`. Internal IDs remain session-only so existing
selection and history behavior can retain identity during edits.

Terrain and mask selection use the layer's array index instead of its internal
ID. Loading reconstructs internal IDs in array order, but rendering does not
depend on their values. Saving and reloading therefore preserve rendering even
after layers have been reordered or deleted. Reordering changes procedural
choices for affected layer indices, which is the accepted tradeoff for removing
persistent identity. Occupancy and mask-grid geometry are unchanged.

This amends the identity and versioning portions of
[Mosaic painted occupancy and procedural rendering](#mosaic-painted-occupancy-and-procedural-rendering).
At the user's explicit direction, these layout and interpretation changes revise
version 4 in place. The current sample is converted. Earlier version-4 layouts
are not supported. See [File formats](../../Mosaic/docs/FILE_FORMATS.md).


## Mosaic terrain tiles and cell coverage

Date: 2026-11-11. Status: Superseded by
[Mosaic whole-texture terrain repetition](#mosaic-whole-texture-terrain-repetition).

Terrain material detail and terrain boundaries need independent spatial scales.
A level uses 64×64-pixel cells for occupancy and 64×64-pixel junction masks for
coverage. Terrain tiles cover one or more cells along each axis, at one source
pixel per world pixel. The terrain grid starts at the map origin.

Every terrain atlas in a level has the same square pixel dimensions. One
level-wide terrain tile size divides those dimensions into an N×N atlas grid,
including 1×1. The tile size is a multiple of 64 pixels. A 4096-pixel terrain tile
covers 64×64 cells. Layer-specific sizes and scaling each texture into one cell
are outside this model. The global dimensions give every layer the same terrain
grid and make atlas compatibility explicit.

Terrain tile selection uses the world terrain tile's row-major index and the
shader's `lowbias32` hash with unsigned 32-bit arithmetic. The hash selects
the atlas tile by remainder. Terrain selection is independent of occupancy and layer identity.
The layer selects its material sheet. Mask selection uses junction occupancy,
coordinates, layer order, and mask settings.

Material exists beneath masks in every cell, including unpainted cells. This
allows soft coverage across cell boundaries. Large terrain tiles preserve
continuous material detail within their footprint. Adjacent terrain tiles
require compatible artwork at their edges.

Version 6 specifies the selection and sampling algorithms as part of the format
contract. Mosaic and the game must agree on unsigned hash arithmetic, map-width
indexing and source coordinates. The document stores occupancy and
level-wide rendering settings, while rendering derives placements without
mutating the document. This follows
[clear responsibility](principles.md#make-it-clear-who-is-responsible).

The authoritative algorithms and reference values are in
[File formats](../../Mosaic/docs/FILE_FORMATS.md#terrain-dimensions-and-selection).
See [Coverage masks](../COVERAGE_MASKS.md) for the relationship between the grids.

## Game entities and level-owned node storage

Date: 2026-11-15. Status: Superseded by
[RAII node handles and stable level ownership](#raii-node-handles-and-stable-level-ownership).

Entities need to create and release nodes, while the level keeps node storage
and shared resources together for animation and rendering. `Level` should not
depend on game-specific entity types. Entities also should not retain a
`Level*` that could become invalid when the level moves.

`Game` owns the level, renderer, and gameplay entities. Entities hold owned
handles for nodes they must release. Other components use borrowed handles
when they need access without responsibility for release. `Level` manages
storage and performs node creation and destruction. It does not own the
gameplay entities.

Release is explicit through `Level`. `Game` supplies its level when releasing
entities' nodes and destroys its entities before destroying the level. Owned
handles transfer release responsibility when moved and diagnose missing
release when destroyed. Their destructors do not release nodes automatically.

We considered storing a level reference in each entity for automatic cleanup,
but that would require a stable level address. Making the level own gameplay
entities would instead require it to support game-specific types through an
interface or type-erased storage. Keeping that ownership in `Game` avoids both
requirements, at the cost of explicit cleanup and a required destruction order.

This follows [clear responsibility](principles.md#make-it-clear-who-is-responsible)
and the former owned and borrowed handles pattern.
See [Game](../../src/Game.h), [Level](../../src/Level.h), and the
[runtime boundary](../development.md#runtime-boundary).

## RAII node handles and stable level ownership

Date: 2026-11-16. Status: Superseded by
[Entity-owned state and sprite submission](#entity-owned-state-and-sprite-submission).

Explicit node release required coordination across entity destruction,
replacement, and partial construction failure. Owned handles tracked release
responsibility but could only diagnose missing cleanup. Automatic release needs
the node owner to remain at a stable address.

`Game` now owns `Level` through `std::unique_ptr`. `Level` cannot be copied or
moved, so transferring the pointer preserves its address. Owned node handles
retain an owner pointer and release their nodes through that owner on
destruction. Moving a handle transfers ownership and leaves the source empty.
Move assignment first releases the destination's node, except for
self-assignment. Explicit early release clears the handle.

Gameplay entities remain owned by `Game`, and `Level` remains independent of
game-specific entity types. `Game` destroys its entities before the level.
Entity members release dependent nodes before the nodes they borrow. Borrowed
handles provide access without extending node or owner lifetimes.

We considered retaining explicit cleanup and extracting a separately allocated
node-storage owner. Retaining explicit cleanup would preserve movable levels
but keep the manual release obligations. Separate storage would provide a stable
address but split responsibilities already held by `Level`. Keeping `Level`
itself stable enables RAII without introducing another storage owner.

Automatic cleanup covers scope exit, replacement, and partial construction
failure. It adds an owner pointer to each owned handle and requires the owner
to outlive every active handle. Dependency order still matters, and moving a
level's ownership now requires moving its pointer rather than the level itself.

This supersedes
[Game entities and level-owned node storage](#game-entities-and-level-owned-node-storage).
It follows [clear responsibility](principles.md#make-it-clear-who-is-responsible)
and the former owned and borrowed handles pattern.
The node implementation described here was later removed. See
[Entity-owned state and sprite submission](#entity-owned-state-and-sprite-submission)
for its replacement.

## Entity-owned state and sprite submission

Date: 2026-11-17. Status: Accepted.

The scene graph required node storage, handles, borrow tracking, and ordered
cleanup for state that entities can own directly.

Entities now own transforms and animation playback state. `Level` keeps shared
resources. Entities queue sprites, and `Renderer` sorts and draws them.

Keeping the scene graph would preserve automatic parent transforms, but retain
its lifetime machinery. Direct ownership is simpler. Attached parts must compose
their transforms explicitly, and sprites must be submitted for each render
attempt.

This supersedes
[RAII node handles and stable level ownership](#raii-node-handles-and-stable-level-ownership).
It follows [clear responsibility](principles.md#make-it-clear-who-is-responsible).
See [Entity state and rendering](patterns.md#entity-state-and-rendering).


## Mosaic whole-texture terrain repetition

Date: 2026-11-18. Status: Accepted.

Terrain artwork now supplies one seamless tile per image. Version 7 removes the
level-wide terrain tile-size setting and procedural atlas selection. Each layer
repeats its entire texture from the map origin at one source pixel per world
pixel. Source coordinates are world coordinates modulo the texture dimension.
Occupancy, junction masks, mask orientations, and compositing remain unchanged.

All terrain textures in a project catalog have identical square dimensions,
from 64 through 4096 pixels per side in multiples of 64. Equal dimensions match
the game's texture-array constraint. Multiples of 64 align texture repeats with
occupancy cells and allow contiguous source rows within each cell. Dimensions
come from the images and are not serialized.

We considered independently sized layer textures and arbitrary image dimensions.
Shared dimensions preserve texture-array compatibility. Restricting dimensions
to multiples of 64 keeps cell sampling simple. This gives up independent material
scales and requires matching, seamless artwork. Whole-texture repetition removes
atlas variation and its selection algorithm.

Mosaic reads and writes version 7 only, without a compatibility loader. The
Mosaic sample adopts the new format. The existing game atlas shader remains
separate runtime work and does not yet implement this contract.

This supersedes [Mosaic terrain tiles and cell coverage](#mosaic-terrain-tiles-and-cell-coverage).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#terrain-dimensions-and-selection)
and [Coverage masks](../COVERAGE_MASKS.md).


## Mosaic decorative prop placement

Date: 2026-11-18. Status: Accepted. Asset discovery and version support are amended by
[Mosaic nested tile and prop assets](#mosaic-nested-tile-and-prop-assets).

Mosaic needs decorative image placement independently of painted terrain. Props
must remain intact when terrain is repainted and participate in the existing
history, asset lifetime, and autosave lifecycle.

Version 8 adds an optional `assets/props/` discovery directory, an ordered prop
image catalog, and an ordered instance list. Instances store an image reference,
a center position in map pixels, and clockwise rotation in degrees. Images keep
their native dimensions and alpha. Props draw above terrain in placement order.
A separate transparent canvas lets dragging redraw props without recomputing
terrain. The document owns placements and snapshots share immutable decoded
images, following the existing [project persistence lifecycle](#mosaic-project-folders-and-http-persistence).

We considered tying props to terrain layers or cells. Independent placements
allow free movement and keep terrain operations from changing decoration. Center
pivots and array draw order avoid asset metadata and ordering controls for this
initial version. They give up custom ground anchors and depth sorting. Picking
uses rotated image rectangles, including transparent pixels.

Mosaic reads and writes version 8 only, with no migration loader. Earlier project
files are rejected and remain untouched. This supersedes version 7 support in
[whole-texture terrain repetition](#mosaic-whole-texture-terrain-repetition), while
preserving its terrain sampling contract. The game does not yet load these props.
Game resource identities and Y-up transforms require a separate integration design.

This follows [clear responsibility](principles.md#make-it-clear-who-is-responsible).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props) and the
[user guide](../../Mosaic/README.md).


## Mosaic nested tile and prop assets

Date: 2026-11-18. Status: Accepted. Version support is superseded by
[Mosaic prop JSON field names](#mosaic-prop-json-field-names).

Asset collections need subfolders to keep terrain and decorative images organized
without flattening filenames. Version 9 allows nested paths under `assets/tiles/`
and `assets/props/`. Mask discovery remains flat.

The browser walks ordinary HTTP directory listings and sorts discovered image
URLs before loading them. Each listing supplies immediate child links, and each
path component is decoded once for the asset's project-relative identity. Labels
retain subfolder names. Existing saved catalog order is preserved, with new assets
appended in discovery order. The server remains independent of editor semantics.
Only a missing top-level props folder is optional. Failures reading discovered
subfolders fail the candidate load and preserve the current document.

We considered requiring users to flatten their folders or using a separate asset
manifest. Recursive discovery keeps folders as the source of truth and avoids a
second catalog to maintain, at the cost of one HTTP listing request per folder.
Terrain dimension constraints and rendering rules remain unchanged.

Mosaic reads and writes version 9 only, without migration support. This amends
asset discovery in [project folders and HTTP persistence](#mosaic-project-folders-and-http-persistence)
and supersedes version 8 support in [decorative prop placement](#mosaic-decorative-prop-placement).
Existing project files are not rewritten as part of this change.

This follows [clear responsibility](principles.md#make-it-clear-who-is-responsible).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#assets-and-limits).


## Mosaic prop JSON field names

Date: 2026-11-18. Status: Accepted. Version support is superseded by
[Mosaic configurable square map size](#mosaic-configurable-square-map-size).

Version 10 names each serialized prop's catalog reference `index` and its
clockwise angle in degrees `rot`, replacing `asset` and `rotation`. Coordinates,
catalog ordering, and rendering semantics remain unchanged. The in-memory
document continues to use `asset` for the decoded image reference and `rotation`
for the angle.

Renaming required fields changes the serialized contract, so the format version
advances rather than changing the meaning of version 9. Mosaic reads and writes
version 10 only, without migration support or aliases for the previous names.
Existing project files are not rewritten as part of this change.

This supersedes version 9 support in
[nested tile and prop assets](#mosaic-nested-tile-and-prop-assets).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic configurable square map size

Date: 2026-11-19. Status: Accepted. Version support and prop coordinate units are
superseded by [Mosaic prop cell coordinates](#mosaic-prop-cell-coordinates).

Map size belongs to the project document rather than global editor constants.
Version 11 permits an integer `gridSize` from 1 to 512, with 64 as the default.
Cells remain 64×64 world pixels. Cell indices use the document's row width,
junction grids have one extra row and column, and prop bounds follow map pixels.

The user-selected maximum is 512 cells per side. The renderer still allocates
full-map canvases, so this bound does not guarantee that a browser can render
the largest maps. At 512 cells, one 32768×32768 RGBA buffer requires 4 GiB.
Separate width and height are not part of this change.
The allocation approach is superseded by
[Mosaic viewport rendering](#mosaic-viewport-rendering).

Resizing is a controller edit using the existing snapshot history and save queue.
It anchors the top-left corner, remaps retained occupancy by coordinates, and
fills new space with base terrain. Shrinking crops occupancy and removes props
whose centers lie beyond the new bottom or right boundary. Clamping those props
would instead relocate and potentially stack decoration, so they are removed.
A confirmation dialog reports both dimensions and the number of props removed.
The entire resize can be undone while its snapshot remains in session history.

Mosaic reads and writes version 11 only, without migration support. This
supersedes version 10 support in [prop JSON field names](#mosaic-prop-json-field-names).
The project2 file receives a version-only update, keeping its 64×64 dimensions
and contents. Runtime consumers must use `gridSize` for bounds and row strides.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md) and the
[user guide](../../Mosaic/README.md).


## Mosaic viewport rendering

Date: 2026-11-19. Status: Accepted.

The map surface provides the scrollable extent without allocating map-sized
pixels. Terrain and prop canvases cover the visible viewport at display-pixel
resolution. Their shared view describes the world origin, display dimensions,
and zoom. Input coordinates continue to use the full map surface, so scrolling
and zooming do not change placement, selection, or painting coordinates.
The SVG grid uses the same visible world rectangle.

Terrain samples display-pixel centers directly from source materials and masks.
Zooming out does not create a larger intermediate image. Dirty cell bounds,
including affected junction neighbors, intersect the visible viewport. Scrolling,
zooming, and viewport layout changes redraw visible terrain, coalesced to an
animation frame. Prop edits retain the existing separation from terrain redraws.
Rotated prop bounds determine visibility, preserving sprites whose centers are
offscreen but whose artwork overlaps the viewport.

The coverage panel retains its whole-map overview, sampled directly at panel
resolution. Scrolling the map does not redraw that overview. Neither view keeps
terrain chunks or rendered map pixels outside its canvas. This trades repeated
sampling during scrolling for bounded canvas memory and straightforward
invalidation. Document and history storage still scale with map content.

This supersedes the full-map canvas allocation described in
[configurable square map size](#mosaic-configurable-square-map-size).
Serialization, cell dimensions, mask rules, and the version 11 format are
unchanged. There are no new dependencies or build requirements.


## Mosaic prop cell coordinates

Date: 2026-11-19. Status: Superseded by
[Mosaic centered coordinates and resizing](#mosaic-centered-coordinates-and-resizing).

Version 12 stores prop image centers in fractional cell coordinates. The origin
remains at the top-left, X increases rightward, and Y increases downward.
Valid coordinates lie between zero and `gridSize`, inclusive. Placement and
movement round to 0.001 cells during editing. Loading and saving use the same
rounding, with validation before rounding so out-of-bounds input is rejected.

The document, prop gestures, box selection, and resize bounds all use cells.
Rendering multiplies coordinates by the 64-pixel cell size. Picking converts
cell offsets to pixels before testing the rotated native image rectangle.
Image dimensions and terrain rendering remain in pixels, and `rot` remains
clockwise degrees. This changes the meaning of serialized coordinates, so
Mosaic reads and writes version 12 only and rejects older versions without
migration support.

The existing project2 file is updated once by dividing its prop coordinates by
64, rounding to three decimal places, and setting version 12. Other content is
preserved. Game-side resources are outside this editor change.

This supersedes the version support and pixel-valued prop coordinates in
[configurable square map size](#mosaic-configurable-square-map-size).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic centered coordinates and resizing

Date: 2026-11-19. Status: Accepted. Version support and prop anchoring are superseded by
[Mosaic prop asset pivots and scale](#mosaic-prop-asset-pivots-and-scale).

Version 13 uses fractional cell coordinates centered on the map, with positive X
rightward and positive Y upward, matching the game's map coordinate convention.
Allowed map sizes are powers of two from 2 through 512. Size 1 is excluded so
centered resizing always moves terrain storage by whole cells. The origin is a
corner shared by the four central cells, whose centers are at plus or minus 0.5.
Prop centers lie within plus or minus half the map size on each axis and retain
three-decimal rounding. Rotation remains clockwise in image space. Scale and
pivot changes are deferred.

Resizing copies the centered terrain overlap, adding or cropping equally on all
four sides. Retained props keep their coordinates. Shrinking removes props beyond
the centered bounds, while keeping centers exactly on the boundary. The existing
confirmation dialog reports these consequences, and snapshots retain the full
prior document for Undo.

Terrain occupancy, procedural mask hashing, and texture repeat sampling retain
their top-left row/column conventions. Resizing remaps occupancy and may change
texture phase or mask variations, which the dialog explains. Rendering converts
centered prop coordinates to top-left map pixels at its boundary. Picking reverses
the Y direction before testing the native image rectangle. No full-map canvas
allocation or terrain caching is introduced.

Mosaic reads and writes version 13 only, without migration support. Project2 is
updated once using `x = oldX - gridSize / 2` and `y = gridSize / 2 - oldY`, keeping
all other content intact. This supersedes the origin in
[prop cell coordinates](#mosaic-prop-cell-coordinates) and the resize anchoring
and size constraints in [configurable square map size](#mosaic-configurable-square-map-size).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic prop asset pivots and scale

Date: 2026-11-19. Status: Accepted. Version support and serialized asset paths are
superseded by [Mosaic project-root asset paths](#mosaic-project-root-asset-paths).

Version 14 changes `propAssets` from image paths to objects containing `texPath`,
`pivotX`, `pivotY`, and `scale`. Each omitted pivot component defaults to 0.5 and
omitted scale defaults to 1. Pivot components are finite normalized coordinates
in [0, 1], measured from the image's upper-left to lower-right corner. Scale must
be finite and positive. Saves write all four fields explicitly.

Each instance's centered, Y-up cell coordinates now locate its asset pivot.
Scaling and clockwise rotation both operate about that fixed point. Drawing,
placement previews, selection outlines, picking, and viewport culling use the
same scaled image bounds relative to the pivot. Box selection and map bounds
use pivots, keeping group movement and resizing anchored consistently.

These settings belong to the asset because all instances of that image share
its anchor and size. Loaded catalog entries retain immutable settings and share
decoded image resources with their source assets. History snapshots continue to
share those entries. Discovery appends new images with default metadata without
changing saved catalog order or settings. Metadata can be edited in JSON and
reloaded. This change adds no editing controls.

Mosaic reads and writes version 14 only, without migration support. Project2 is
converted once with explicit center pivots and scale 1, preserving its existing
placements. This supersedes the version support and center-only anchoring in
[centered coordinates and resizing](#mosaic-centered-coordinates-and-resizing).
Map coordinates, terrain rendering, and instance rotation fields remain unchanged.
The C++ loader is outside this change and still expects the previous string catalog.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic project-root asset paths

Date: 2026-11-19. Status: Accepted. Version support is superseded by
[Mosaic prop collision bounds](#mosaic-prop-collision-bounds).

The game's root is the project directory, while Mosaic serves projects beneath
`/projects/<name>/`. Version 15 requires a leading slash in `tileSheets`,
`maskSheets`, and `propAssets[].texPath`, such as `/assets/props/tree.png`, so the
saved paths are directly rooted at the game's project directory.

Mosaic removes that slash when matching paths to discovered assets. Its in-memory
IDs remain project-relative, and its HTTP requests still resolve within the
opened project. The path above therefore loads through
`/projects/project2/assets/props/tree.png` when project2 is open. Existing filename
validation and the 1024-character limit on the relative portion remain unchanged.

Mosaic reads and writes version 15 only, without migration support. Project2's
asset paths and version are converted once, preserving all other data. This
supersedes version support and serialized path syntax in
[prop asset pivots and scale](#mosaic-prop-asset-pivots-and-scale). Prop metadata,
terrain rules, and catalog ordering are unchanged. No C++ changes are included.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#assets-and-limits).


## Level-owned decorative props

Date: 2026-11-19. Status: Accepted.

`Level` owns decorative props because their placement and lifetime belong to the
loaded level. `Game` retains gameplay entities and coordinates sprite submission
for both. This extends the
[entity-owned state and sprite submission decision](#entity-owned-state-and-sprite-submission).


## Mosaic prop collision bounds

Date: 2026-11-19. Status: Accepted. Version support is superseded by
[Mosaic prop position objects](#mosaic-prop-position-objects). The field name is
superseded by [Mosaic prop collider naming](#mosaic-prop-collider-naming).

Version 16 groups serialized prop pivots into `pivot: { x, y }` and adds optional
asset-owned `bounds` arrays of rectangles with `x`, `y`, `w`, and `h`.
Bounds use normalized sprite coordinates from the upper-left corner. This keeps
bounds stable when artwork resolution changes and matches normalized pivots.
Pixel coordinates were considered but would couple bounds to image resolution.
Positive dimensions are required. Overlap and coordinates outside the image are
allowed. Missing bounds mean no collision. Bounds follow the sprite transform.

Metadata remains immutable and shared by history snapshots. Bounds are authored
in JSON and displayed through a session-only overlay toggle. Picking continues
to use image rectangles. Game collision behavior is separate work. Game loader compatibility with nested pivots is handled separately.

Mosaic reads and writes version 16 only, without migration support or legacy
pivot aliases. Project2 is converted while preserving pivots and placements and
adding empty bounds. This supersedes version support in
[project-root asset paths](#mosaic-project-root-asset-paths) and the serialized
pivot structure in [prop asset pivots and scale](#mosaic-prop-asset-pivots-and-scale).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic prop position objects

Date: 2026-11-19. Status: Accepted. Version support is superseded by
[Mosaic prop source rectangles](#mosaic-prop-source-rectangles).

Version 17 groups each serialized prop's position into required `pos: { x, y }`,
replacing the flat `x` and `y` fields. This makes position an explicit value,
consistent with the asset's nested pivot. Keeping flat fields was an alternative,
but would retain different JSON shapes for these coordinate pairs.

Centered Y-up cell coordinates, bounds validation, three-decimal rounding, and
instance rotation are unchanged. The editor retains its existing in-memory
coordinates. Mosaic reads and writes version 17 only, with no migration loader
or flat-position aliases. Project2 is converted without changing position values
or unrelated data. Game loader compatibility is separate work.

This supersedes version support in
[prop collision bounds](#mosaic-prop-collision-bounds).
See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic prop source rectangles

Date: 2026-11-20. Status: Accepted. Version support and full-texture serialization
are superseded by [Mosaic full-texture source rectangle shorthand](#mosaic-full-texture-source-rectangle-shorthand).

Version 18 adds asset-owned `srcRect: { x, y, w, h }` in source texture pixels.
Omission selects the full decoded texture, and saves write the resolved rectangle
explicitly. Components are integers, coordinates nonnegative, and dimensions
positive. The rectangle must fit within the texture. Pixel coordinates match the
game renderer's source rectangles and make exact artwork selection straightforward.
Normalized texture coordinates were considered, but would make pixel boundaries
less direct to author.

Pivots and collision bounds remain normalized, now relative to the cropped sprite.
Scale applies to the crop's dimensions. Drawing, palette and placement previews,
picking, selection outlines, viewport culling, and collision overlays use that
same sprite geometry. Source offsets select texture pixels without moving the
world pivot. Decoded textures stay shared, while immutable asset metadata remains
owned by catalog entries and shared by history snapshots. Metadata is edited in
JSON and reloaded. The existing unique-texture-path catalog rule is unchanged.

Mosaic reads and writes version 18 only, without a migration loader. Project2
receives only a version update, preserving its full-texture appearance through
the default. This supersedes version support in
[prop position objects](#mosaic-prop-position-objects) and extends the whole-image
geometry in [prop asset pivots and scale](#mosaic-prop-asset-pivots-and-scale) and
[prop collision bounds](#mosaic-prop-collision-bounds). C++ loader integration is
separate work. There are no dependency or build requirement changes.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic full-texture source rectangle shorthand

Date: 2026-11-20. Status: Accepted. Version support is superseded by
[Mosaic prop collider naming](#mosaic-prop-collider-naming).

Version 19 uses `srcRect: []` for the entire texture. Omission also selects the
full texture, and explicit rectangle objects remain valid. Saves normalize any
rectangle that covers the entire texture to `[]`, while cropped sprites retain
`{ x, y, w, h }`. Nonempty arrays and `null` are rejected.

The shorthand keeps full-texture entries compact and lets them follow changes to
the source image dimensions. Always storing explicit dimensions was an
alternative, but would keep those entries tied to the previous image size.
The document still resolves a concrete pixel rectangle on load, so rendering,
picking, pivots, collision bounds, and history retain their existing behavior.

Mosaic reads and writes version 19 only, without migration support. Version 18
rejects empty arrays, so the format version advances with this change. Project2
receives only a version update and continues to use the full-texture default.
This supersedes version support and full-texture serialization in
[prop source rectangles](#mosaic-prop-source-rectangles). C++ integration remains
separate work. No dependencies or build requirements change.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic prop collider naming

Date: 2026-11-21. Status: Superseded by
[Mosaic collision rectangle naming](#mosaic-collision-rectangle-naming).

Version 20 renames the prop asset field `bounds` to `collider`. The name identifies
collision geometry separately from image, selection, and map bounds. Keeping
`bounds` was an alternative, but would retain that ambiguity.

The collider remains an array of normalized sprite-local rectangles with `x`,
`y`, `w`, and `h`. Omission defaults to `[]`, meaning no collision. Validation,
pivot and scale transforms, and overlay rendering are unchanged. The editor's
toggle is now Show collider. Picking continues to use the sprite rectangle.

Mosaic reads and writes version 20 only, without migration support or a legacy
field alias. Project2's asset keys and version are updated while preserving all
values. This supersedes version support in
[full-texture source rectangle shorthand](#mosaic-full-texture-source-rectangle-shorthand)
and the field name in [prop collision bounds](#mosaic-prop-collision-bounds).
C++ loader integration remains separate work. No dependencies or build
requirements change.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic collision rectangle naming

Date: 2026-11-21. Status: Accepted.

Version 21 renames `collider` to `collisionRects`. The name identifies both the
collision purpose and the array of rectangles. `collider` suggests a broader
collision object, while `cbounds` is less explicit and does not communicate the
array contents. The UI toggle is Show collision rectangles.

Each rectangle retains finite `x`, `y`, `w`, and `h`, with positive dimensions.
Coordinates are normalized relative to the cropped sprite before scale and
rotation: `(0, 0)` is upper-left and `(1, 1)` is lower-right. Rectangles may overlap
or extend outside that range. Omission or `[]` means no collision. Validation,
rendering, picking, and history behavior are unchanged.

Mosaic reads and writes version 21 only, without migration support or legacy
field aliases. Project2's keys and version are updated while preserving values.
This supersedes [prop collider naming](#mosaic-prop-collider-naming). C++ loader
integration remains separate work. No dependencies or build requirements change.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#decorative-props).


## Mosaic opaque project extensions

Date: 2026-11-21. Status: Accepted.

The optional top-level `extensions` field carries application-owned JSON that
Mosaic preserves without interpreting or editing. The document owns the loaded
value. History and save snapshots share it as recursively readonly data, following
the existing immutable asset ownership pattern without copying it for each edit.
Omission stays omitted, and explicit `null` stays present.

Preservation covers parsed JSON values, with JavaScript numeric precision, rather
than source formatting. Other unknown fields remain outside this contract.
Version 21 is retained because this optional field does not alter existing fields'
layout or meaning. Older Mosaic builds can open these files but discard extensions
when saving. No server, runtime loader, dependency, or build changes are required.

See [File formats](../../Mosaic/docs/FILE_FORMATS.md#level-json).

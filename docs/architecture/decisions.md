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

Date: 2026-11-11. Status: Accepted.

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

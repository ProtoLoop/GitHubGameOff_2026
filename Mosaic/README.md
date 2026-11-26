# Mosaic

Create a project folder under `projects/` at the repository root, alongside `Mosaic/`:

```text
my-project/
  assets/
    tiles/       # equally sized square seamless terrain textures
    masks/       # 256×256 alpha-mask sheets
    props/       # optional discovered sprite images, including transparency
  level.json     # created when you save
```

Put PNG, JPEG, or WebP images in the appropriate folders. Tiles and props may
use nested subfolders at any depth. Masks must be directly inside `assets/masks/`. All terrain
textures in a level must be square, have identical pixel dimensions, and be
64–4096 pixels per side in multiples of 64. Each entire texture is one tile.

One texture pixel covers one map pixel. Textures repeat from the map origin.
A 256×256 texture covers 4×4 cells per repeat. A 1024×1024 texture covers
16×16 cells, or 16 times the area per repeat. Opposite texture edges must be
seamless. Layer details show texture dimensions and cell coverage. Changing a
layer's material preserves painted cells and can be undone.

Masks use the [4×4 coverage layout](../docs/COVERAGE_MASKS.md). The main
project is `project2`. Copy its asset folders to make a new project.
Project2 contains 48 sequences grouped into 27 sprites: 24 static sprites and
three directional actor sprites. The referenced actor textures are present in the
project. There is no fallback to repository-level assets.
Mosaic reads and writes version 28 only, with no migration support. Existing
level files from earlier versions cannot be opened by this version. A project without `level.json` starts a new level.

Asset paths in `level.json` start with `/assets/`, relative to the game's root—the
project directory. Mosaic resolves those paths within the opened project. For
example, `/assets/props/tree.png` in project2 is loaded through
`/projects/project2/assets/props/tree.png`.

Build from the workspace root with CMake 3.27+, Ninja, and Python 3.9+:

```sh
cmake -S Mosaic -B Mosaic/build -G Ninja
cmake --build Mosaic/build
python3 Mosaic/server.py --projects-root projects
```

CMake provisions Deno automatically.

Open **http://localhost:8000/**. Stop any other server using port 8000 first,
or pass `--port 8001` and open http://localhost:8001/.

The top toolbar keeps project actions, undo/redo, and save status visible while
you edit. Materials, layers, and map settings are in the left sidebar, with
instructions under **Help**. The map and selected-layer coverage preview occupy
the right side. The sidebar and map scroll independently. On smaller windows,
the toolbar wraps and the coverage preview moves below the map.

Map canvases cover only the visible viewport. Scrolling and zooming redraw that
view directly, without cached terrain chunks or full-map pixel buffers. The
coverage preview remains a whole-map overview, sampled at its displayed size.
Canvas memory depends on window size. Map data and Undo snapshots still grow
with the number of cells, layers, and placed objects.

The app starts in **Select** mode. Later project switches and reloads retain the current tool.

- Click **Open Project**, select a project in the chooser, then click **Open**.
  **Refresh Projects** updates the list. **Cancel** or Escape closes the chooser.
- **Reload Project** picks up external level or asset changes, with no rebuild.
- Under **Map settings**, choose **Map size (cells per side)** from
  2, 4, 8, 16, 32, 64, 128, 256, or 512, then click **Apply**. Maps are square and cells stay 64×64 pixels.
  New projects start at 64×64 cells. The confirmation dialog shows old and new
  cell/pixel dimensions and explains the consequences. Cancel is focused initially.
  Growing adds base terrain equally on all four sides, keeping the map center fixed.
  Shrinking crops terrain equally on all four sides and removes props and actors outside the new bounds.
  Retained objects keep their coordinates. Terrain repeats and mask variations are
  regenerated using the resized grid. The dialog reports how many props and actors will be removed. Objects with pivots exactly on an
  edge remain, with their artwork clipped there. **Resize map** applies one
  undoable, autosaved change. Undo restores the previous size, cropped terrain,
  and removed objects within the editor's bounded, session-only history.
- Select a swatch and drag left to paint. Right-click erases the selected overlay.
- Choose a prop or actor palette swatch for placement or a terrain swatch for painting.
  The active swatch is highlighted. Escape cancels the active gesture and always
  returns to object selection, clearing palette highlights while retaining the last
  choices. Object selection uses the move cursor, placement uses the default arrow,
  and terrain painting uses a crosshair. Middle-button panning uses the grabbing cursor.
- Terrain painting shows a 50%-opacity preview of the selected whole texture,
  centered under the cursor at the palette thumbnail's displayed size (56×56 pixels,
  or 42×42 on narrow windows), independent of map zoom. It follows scrolling and
  disappears outside the map or after Escape. Painting still affects individual
  cells. The preview does not change terrain sampling or brush size.
- Choose a prop or actor swatch to enter **Place**. A translucent preview follows the cursor
  at the asset scale, with its pivot under the cursor. Left-click places one copy and stays in Place for repeated
  stamping. Escape or right-click returns to Select, keeping the last copy selected.
  Choose a prop or actor swatch again to resume placement. Previewing does not save or create
  undo actions. Add PNG, JPEG, or WebP images anywhere under `assets/props/` and
  reload to add sprite resources. Add definitions in JSON to make them placeable. Rectangular images support 1–4096 pixels per axis.
- `sequences` is a catalog of named image sequences. Each entry stores `name`,
  `texPath`, `srcRect`, `pivot`, `scale`, `tint`, and `collisionRects`, with optional
  `size: { w, h }` specifying one frame's pixel dimensions. Omit `size` to use the
  full resolved source rectangle as one frame.
  Sequence names must be unique, nonblank, and at most 63 UTF-8 bytes, without
  control characters. Unnamed entries use their filename without its extension.
  Saves write the name explicitly and preserve it on reload. Name collisions
  require explicit names in JSON. Multiple sequences may share one texture.
  `tint: { r, g, b, a }` multiplies each color and alpha channel, with components
  that are finite and non-negative, with no upper limit. Omission means white. Edit metadata in JSON and reload.
  Referenced textures may be anywhere under the project's `assets/` directory.
  Automatic discovery still appends only images under `assets/props/` that are
  not already referenced by the catalog.
  The optional `srcRect: { x, y, w, h }` selects a rectangle in texture pixels
  from the upper-left corner. Omission or `{}` selects the full texture. Components must
  be integers, dimensions positive, and the rectangle entirely inside the texture.
  Saves write `srcRect: {}` for the full texture and an object for a cropped sprite.
  Pivots are normalized from one frame's upper-left `(0, 0)` to lower-right
  `(1, 1)`, defaulting to `(0.5, 0.5)`. Scale is finite and positive, defaulting to
  `1`. Scale and rotation both operate about the pivot. Edit these asset settings
  in JSON and reload the project to apply them.
- `sprites` groups sequences by facing direction. Each entry has a unique `name`
  and a nonempty ordered `sequences` array of sequence names. Mosaic
  displays frame zero of each sprite's first sequence. Rotation still rotates
  that image. Size, pivot, scale, tint, and collision rectangles stay on sequences.
  Reordering either top-level catalog does not change references. Edit sprite
  groups in JSON and reload. Newly discovered images each get one sequence and
  one same-named sprite. Name conflicts fail loading.
- **Props** lists `propTypes` entries with `name` and `sprite`. **Actors** lists
  `actorTypes` entries with `name`, `class`, and `sprite`. The sprite reference
  selects the appearance. The actor class identifies runtime behavior, which Mosaic
  does not execute. Both catalogs are explicit and maintained in JSON. Discovered
  sprites are not automatically placeable, and no filename or animation rules
  assign them to a palette. Multiple definitions may share a behavior type.
- `props` placements reference definitions through `type: "name"`, and `actors`
  through `type: "name"`. Both store `pos: { x, y }` and `rot`. Names are unique
  within each definition catalog. Definitions initially use the same name as their
  sprite, but this is not required. Actors show a green square at their pivot,
  including during placement preview. Actor previews draw above props.
- Add normalized collision rectangles to a sequence’s `collisionRects` array using `x`, `y`,
  `w`, and `h`. Omitted `collisionRects` or `[]` means no collision. Dimensions must be
  positive, and rectangles may extend outside the cropped sprite. Collision rectangles use the
  frame dimensions for normalization: `(0, 0)` is the upper-left corner
  and `(1, 1)` the lower-right, before scale and rotation. Edit JSON and reload,
  then enable **Show collision rectangles** to see cyan rectangles on placed props and actors. The toggle
  is session-only. Collision rectangles follow pivot, scale, and rotation without changing picking.
- In **Select**, click a prop or actor to select it. Shift-click adds or removes it from the
  selection. Drag empty space to box-select objects whose pivots lie inside the box.
  Shift-drag empty space adds those objects to the selection. Clicking without dragging
  selects only the clicked object, while dragging a selected object moves the group.
- Selected objects have outlines and a magenta crosshair with a dark border at each
  pivot. Crosshairs stay upright and the same screen size at every zoom. The sidebar
  shows the selection count. Delete or Backspace removes the group as one undo action.
  Escape cancels an active move or box selection, otherwise it clears selection.
  Picking uses scaled, rotated image rectangles, including transparent pixels. Later
  placements within each palette category draw on top.
- Props and actors move freely in fractional cell coordinates, rounded to three decimal
  places during editing and saving. `(1.5, 2.25)` places the image pivot 1.5 cells
  right and 2.25 cells up from the map center. `(0, 0)` is the corner shared by
  the four central cells. Each cell spans 64 pixels. Their
  pivots stay within the map, with group spacing preserved at the edges. Artwork
  is clipped at map edges. Each drag is
  one undo action. Choose a material swatch to resume painting.
- Drag the middle mouse button to pan. Use the wheel to scroll. Hold Alt (Option on Mac) and use the wheel to zoom around the cursor.
- Edits autosave after a short pause. Ctrl/Cmd+S saves immediately or retries a failed save.
- Hover over controls for tooltips. Palette tooltips show the full project-relative path. Asset labels include
  subfolder names to distinguish identical filenames.
- Ctrl/Cmd+Z undoes. Shift+Ctrl/Cmd+Z redoes. Opening or reloading clears history.

Saving writes only `level.json`. Images stay in their folders. Switching projects
first saves pending edits. A failed save keeps the current project open for retry.
Use one editor per project because concurrent saves overwrite each other.

Run checks with `cmake --build Mosaic/build --target check`.
See [FILE_FORMATS.md](docs/FILE_FORMATS.md) for the JSON and HTTP contract.

Mosaic reads and writes version 28 only, without migration support. The game loads
prop and actor placements from their type catalogs. Actors currently support the
`Tombstone` class. Unsupported classes are logged and skipped. See the
[runtime boundary](../docs/development.md#runtime-boundary) for details.

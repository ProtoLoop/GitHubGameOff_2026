# Mosaic

Create a project folder under `Mosaic/projects/`:

```text
my-project/
  assets/
    tiles/       # square tile sheets
    masks/       # 256×256 alpha-mask sheets
  level.json     # created when you save
```

Put PNG, JPEG, or WebP images directly in the appropriate folders. Tile sheets
must divide into an even grid of at least 2×2 tiles (initial tile size: 64 pixels).
Masks use the [4×4 coverage layout](../docs/COVERAGE_MASKS.md). The included
`sample` project provides assets. Copy a project folder to make another project.

Build from the workspace root with CMake 3.27+, Ninja, and Python 3.9+:

```sh
cmake -S Mosaic -B Mosaic/build -G Ninja
cmake --build Mosaic/build
python3 Mosaic/server.py --projects-root Mosaic/projects
```

CMake provisions Deno automatically.

Open **http://localhost:8000/**. Stop any other server using port 8000 first,
or pass `--port 8001` and open http://localhost:8001/.

The top toolbar keeps project actions, undo/redo, and save status visible while
you edit. Materials, layers, and map settings are in the left sidebar, with
instructions under **Help**. The map and selected-layer coverage preview occupy
the right side. The sidebar and map scroll independently. On smaller windows,
the toolbar wraps and the coverage preview moves below the map.

- Click **Open Project**, select a project in the chooser, then click **Open**.
  **Refresh Projects** updates the list. **Cancel** or Escape closes the chooser.
- **Reload Project** picks up external level or asset changes, with no rebuild.
- Select a swatch and drag left to paint. Right-click erases the selected overlay.
- Drag the middle mouse button to pan. Use the wheel to scroll. Hold Alt (Option on Mac) and use the wheel to zoom around the cursor.
- Edits autosave after a short pause. Ctrl/Cmd+S saves immediately or retries a failed save.
- Hover over controls for tooltips. Palette swatches show the parent folder and filename.
- Ctrl/Cmd+Z undoes. Shift+Ctrl/Cmd+Z redoes. Opening or reloading clears history.

Saving writes only `level.json`. Images stay in their folders. Switching projects
first saves pending edits. A failed save keeps the current project open for retry.
Use one editor per project because concurrent saves overwrite each other.

Run checks with `cmake --build Mosaic/build --target check`.
See [FILE_FORMATS.md](docs/FILE_FORMATS.md) for the JSON and HTTP contract.

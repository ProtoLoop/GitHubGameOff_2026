# Coverage masks

Coverage masks are centered on the junctions where four terrain cells meet.
Each mask covers a 64×64-pixel square, offset by half a cell in both axes from
the terrain tile grid. Its four quadrants overlap the four adjoining cells.
The material texture remains aligned to the terrain tile grid.

## Pattern bits

When an edit changes occupancy, Mosaic computes the affected overlay junction
patterns from painted-cell occupancy:

```text
 NW cell | NE cell
  bit 0  | bit 1
---------+---------
  bit 2  | bit 3
 SW cell | SE cell
```

A set bit means that cell is occupied in the layer. A clear bit means it is
empty or outside the map. All occupied cells in a layer use that layer's
material. Pattern 0 is fully transparent and pattern 15 is fully opaque.

For junction coordinates `(jx, jy)`, the four cells are:

| Bit | Cell |
| --- | --- |
| 0 — NW | `(jx - 1, jy - 1)` |
| 1 — NE | `(jx, jy - 1)` |
| 2 — SW | `(jx - 1, jy)` |
| 3 — SE | `(jx, jy)` |

The junction is at pixel `(jx × 64, jy × 64)`. Its mask begins 32 pixels left
and 32 pixels above that point. A 64×64-cell map has 65×65 junctions. Masks at
the map perimeter are clipped to the map bounds.

## Atlas lookup

Each PNG is 256×256 pixels: a 4×4 grid of 64×64 masks. Patterns 0 through 15
are ordered left to right, then top to bottom. Every image in a project’s `assets/masks/` must use this layout. Mosaic discovers
these files through the folder’s HTTP directory listing.

```text
column = pattern % 4
row    = floor(pattern / 4)
```

Coverage is read from the PNG's red channel, with 0 transparent and 255 opaque.
The PNG alpha channel is not used. Grayscale RGB values express the coverage.
The loader requires the exact sheet dimensions.

## Sampling and compositing

Every terrain cell receives a quarter of each of its four surrounding masks.
For a map pixel `(x, y)`, its junction and mask coordinates are:

```text
jx = floor((x + 32) / 64)
jy = floor((y + 32) / 64)
maskX = (x + 32) % 64
maskY = (y + 32) % 64
```

The selected mask supplies alpha for the layer's material at that pixel.
Layers composite from bottom to top over the accumulated lower layers, so the
base can show through multiple masks.

Every layer supplies a deterministic terrain texture in every cell, including
unpainted cells. Masks alone determine overlay visibility. Pattern 0 supplies
zero coverage. Both previews follow the same rules. The base is an unmasked,
continuous fill. This restores the behavior before version 3 clipped material
at unpainted-cell boundaries and introduced hard edges through soft masks.

## Variants and transformations

Mosaic selects from its mask-sheet catalog using a stable hash of junction
coordinates and layer array index modulo the number of sheets, or uses the sheet selected
in the interface. New levels order sheets by path. Saved levels preserve catalog
order and append newly discovered sheets. Adding sheets changes random selection
throughout the level. Fixed selection is unaffected. Reordering layers changes
procedural choices for layers whose array indices change. Removing or renaming a
referenced sheet causes a load error. Use Reload Project after changing
`assets/masks/`.

Orientation is selected using junction coordinates, layer array index, and the four-bit
pattern. Candidates are the eight unique square symmetries: rotations in
90-degree steps and reflections, including rotation/reflection combinations.
A candidate is eligible only if it preserves all four pattern bits. Identity
is always eligible. Returning to the same pattern restores the same automatic
orientation. Terrain flips are independently chosen from cell coordinates and
layer array index for every terrain cell, regardless of whether it is painted.

Only painted cells and rendering settings are saved. Terrain indices, mask
patterns, sheet choices, and orientations are derived by both editor and runtime.
Painting or erasing changes the four affected junction patterns. Undo and redo
restore occupancy and settings, which reproduce the rendering choices.

Mask orientation uses source-axis flips followed by clockwise rotation. See
[File formats](../Mosaic/docs/FILE_FORMATS.md) for exact selection, sampling,
and transform conventions.

## Mask artwork requirements

Adjacent masks meet along lines connecting terrain-cell centers. Their shared
edge profiles must match for the same occupancy of the two cells on that edge:

- The top edge depends on NW and NE.
- The bottom edge depends on SW and SE.
- The left edge depends on NW and SW.
- The right edge depends on NE and SE.

These profiles must agree across patterns, mask sets, and eligible rotations
and reflections, including the reversed profiles produced by a transformation.
Variation belongs in the mask interiors while the shared profiles remain
compatible. Pattern preservation alone does not guarantee matching artwork.

Patterns 6 and 9 contain diagonally opposed occupied cells. Their artwork
determines whether coverage connects through the junction or stays separated.
Pattern 0 contains only zeros and pattern 15 contains only 255 values.

Painting changes the four junction patterns touching the edited cell. The
renderer refreshes that cell and its surrounding cells in both previews to
cover the affected mask regions.

constexpr const char* const kTileMapShaderCode = R"(
struct VSOut
{
    @builtin(position)  pos : vec4f,
    @location(0)        uv  : vec2f,
    @location(1)        brectUV : vec2f, // Used to draw grid outlines
    @location(3) @interpolate(flat) texLayer : u32,
};

struct CellMapParams
{
    proj : mat2x2f, // projection matrix (isometric, orthographic, etc)
    viewXY : vec2f, // Camera view position in world space
    cellMapCellsPerSide : u32, // Number of cells along each axis in the map
    pad1 : u32,
};

@group(0) @binding(0) var<uniform> cellMapParams : CellMapParams;

@group(1) @binding(0) var tileSheets : texture_2d_array<f32>;
@group(1) @binding(1) var smp : sampler;

override logicalTargetWidth: f32;   // Logical width of the render target in pixels
override logicalTargetHeight: f32;  // Logical height of the render target in pixels
override cellPixelsPerSide: u32;    // Number of pixels along each axis of a cell
override sheetTilesPerSide: u32;    // Number of tiles along each axis in the tile sheet
    
const cellVerts = array<vec2f, 6>(
    vec2f(0.0, 0.0),
    vec2f(1.0, 0.0),
    vec2f(1.0, -1.0),

    vec2f(0.0, 0.0),
    vec2f(1.0, -1.0),
    vec2f(0.0, -1.0)
);

const cellUVs = array<vec2f, 6>(
    vec2f(0.0, 0.0),
    vec2f(1.0, 0.0),
    vec2f(1.0, 1.0),

    vec2f(0.0, 0.0),
    vec2f(1.0, 1.0),
    vec2f(0.0, 1.0)
);

// lowbias32 hash function for generating pseudo-random numbers from a 32-bit integer input.
fn hash(value: u32) -> u32
{
    var h = value;
    h = (h ^ (h >> 16u)) * 0x7feb352du;
    h = (h ^ (h >> 15u)) * 0x846ca68bu;
    return h ^ (h >> 16u);
}

@vertex
fn vs_main(@builtin(vertex_index) vidx : u32, @builtin(instance_index) worldCellIndex: u32) -> VSOut
{
    // The cell index identifies a cell in the map, including its layer.
    // The number of cells in a map is cellMapCellsPerSide * cellMapCellsPerSide * layerCount.

    // The upper left corner of the map is at (-cellMapCellsPerSide/2, cellMapCellsPerSide/2).
    // The lower right is at (cellMapCellsPerSide/2, -cellMapCellsPerSide/2)
    // The origin (0,0) is at the center of the map.

    let cellsPerLayer = cellMapParams.cellMapCellsPerSide * cellMapParams.cellMapCellsPerSide;
    let layer = worldCellIndex / cellsPerLayer;
    // Index of the cell within its layer.
    let layerCellIndex = worldCellIndex % cellsPerLayer;

    let cellVtx = cellVerts[vidx];
    let cellUV = cellUVs[vidx];

    let halfMapGridSize = f32(cellMapParams.cellMapCellsPerSide) / 2.0;

    // Column and row, measured from the map’s top-left with Y down
    let mapCellXY = vec2u(layerCellIndex % cellMapParams.cellMapCellsPerSide, layerCellIndex / cellMapParams.cellMapCellsPerSide);

    // The camera is rounded in projected logical pixels.
    // Matching this in the sprite shader keeps camera movement consistent across shaders
    // and avoid sub-pixel jitter.
    let cameraPixels = round(cellMapParams.proj * cellMapParams.viewXY * f32(cellPixelsPerSide));

    // Top-left corner in centered (origin at map center), Y-up world-cell coordinates.
    let worldXY = vec2f(f32(mapCellXY.x) - halfMapGridSize, halfMapGridSize - f32(mapCellXY.y));

    // Position of the vertex (corner of the cell) in screen space.
    let screenVertXY = cellMapParams.proj * (cellVtx + worldXY) * f32(cellPixelsPerSide) - cameraPixels;
    
    let ndc = (screenVertXY / vec2f(logicalTargetWidth, logicalTargetHeight)) * vec2f(2.0, 2.0);

    // Dimensions of the tile sheet in pixels.
    let sheetPixelSize = textureDimensions(tileSheets).x;
    // Number of pixels along each axis of a tile.
    // Sheets should be a multiple of tile size.
    let tilePixelSize = sheetPixelSize / sheetTilesPerSide;
    // Number of cells along each axis of a tile.
    let tileCellSize = tilePixelSize / cellPixelsPerSide;

    // A single tile can be >= the size of a cell, but should be a multiple of the cell size.
    // A single sheet/atlas contains 1 or more tiles, but should be square,
    // and should be a multple of the tile pixel size.

    // If we imagine the map is not only a grid of cells, but also
    // a grid of tiles, then we calculate the index of the tile in the tile map.

    // Coords of the tile containing the cell in tile map space.
    let mapTileXY = mapCellXY / tileCellSize;
    let tileColumnCount = u32(ceil(f32(cellMapParams.cellMapCellsPerSide) / f32(tileCellSize)));
    // Index of the tile in the tile map.
    let mapTileIndex = mapTileXY.y * tileColumnCount + mapTileXY.x;

    // Choose a deterministically random tile within the sheet.
    let sheetTileIndex = hash(mapTileIndex) % (sheetTilesPerSide * sheetTilesPerSide);

    // Coords of the cell in tile space (within the tile).
    // IOW, if a tile contains 4 × 4 cells, then a cell's position within
    // the tile would be (0,0) for the top-left cell, (1,0) for the cell to its right, etc.
    let tileCellXY = vec2f(f32(mapCellXY.x % tileCellSize), f32(mapCellXY.y % tileCellSize));
    // Position of the vertex (corner of the cell) within the tile.
    var tileVertXY = tileCellXY + cellUV;

    // Coords of the tile in sheet space (within the sheet).
    let sheetTileXY = vec2f(f32(sheetTileIndex % sheetTilesPerSide), f32(sheetTileIndex / sheetTilesPerSide));

    // UV coordinates of the vertex within the tile.
    let tileVertUV = tileVertXY / vec2f(f32(tileCellSize));

    return VSOut(
        vec4f(ndc, 1.0, 1.0), // pos
        (sheetTileXY + tileVertUV) / vec2f(f32(sheetTilesPerSide)), // uv
        cellUV, // gridUV
        layer % textureNumLayers(tileSheets), // texLayer
    );
}

fn linear_to_srgb(linear: vec4f) -> vec4f
{
    let lo = vec4f(linear.rgb * 12.92, linear.a);
    let hi = vec4f(1.055 * pow(max(linear.rgb, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055, linear.a);

    return select(hi, lo, vec4(linear.rgb <= vec3f(0.0031308), true));
}

// Returns the grid line color when the fragment coincides with the top/left edges of a cell.
fn drawBoundingRect(gridUV: vec2f, color: vec4f) -> vec4f
{
    let thickness = fwidth(gridUV);
    return select(color, vec4f(0.0, 1.0, 1.0, 1.0), any(gridUV < thickness));
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4f
{
    var p = textureSample(tileSheets, smp, i.uv, i.texLayer);
    //p = drawBoundingRect(i.brectUV, p);
    return p;
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4f
{
    var p = textureSample(tileSheets, smp, i.uv, i.texLayer);
    //p = drawBoundingRect(i.brectUV, p);
    return linear_to_srgb(p);
}
)";
constexpr const char* const kTileMapShaderCode = R"(
struct VSOut
{
    @builtin(position)  pos : vec4f,
    @location(0)        uv  : vec2f,
    @location(1)        gridUV : vec2f, // Used to draw grid outlines
    @location(3) @interpolate(flat) texLayer : u32,
};

struct CellMapParams
{
    viewXY : vec2f, // Camera view position in world space
    pad0 : u32,
    pad1 : u32,
};

@group(0) @binding(0) var tileSheets : texture_2d_array<f32>;
@group(0) @binding(1) var smp : sampler;
@group(0) @binding(2) var<uniform> cellMapParams : CellMapParams;

override logicalTargetWidth: f32;   // Logical width of the render target in pixels
override logicalTargetHeight: f32;  // Logical height of the render target in pixels
override cellPixelSize: u32;        // Number of pixels along each axis of a cell
override sheetGridSize: u32;        // Number of tiles along each axis in the tile sheet
override cellMapGridSize: u32;      // Number of cells along each axis in the map
    
const cellVerts = array<vec2f, 6>(
    vec2f(0.0, 0.0),
    vec2f(1.0, 0.0),
    vec2f(1.0, 1.0),

    vec2f(0.0, 0.0),
    vec2f(1.0, 1.0),
    vec2f(0.0, 1.0)
);

// Isometric transformation matrix.
const iso = mat2x2f(vec2f(0.5, 0.25), vec2f(-0.5, 0.25));

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
    // The cell index identifies a cell in the map, including it's layer.
    // The number of cells in a map is cellMapGridSize * cellMapGridSize * layerCount.

    // The map extends from -cellMapGridSize/2 to +cellMapGridSize/2 in both x and y directions.
    // The origin (0,0) is at the center of the map.
    // The upper left corner of the map is at (-cellMapGridSize/2, -cellMapGridSize/2) in world space.

    let cellsPerLayer = cellMapGridSize * cellMapGridSize;
    let layer = worldCellIndex / cellsPerLayer;
    // Index of the cell within its layer.
    let layerCellIndex = worldCellIndex % cellsPerLayer;

    let cellVtx = cellVerts[vidx];

    // Coords of the cell in cell map space
    let mapCellXY = vec2u(layerCellIndex % cellMapGridSize, layerCellIndex / cellMapGridSize);
    // Coords in camera space
    let camXY = vec2f(mapCellXY) - cellMapParams.viewXY;
    // Vertex position in screen space
    //let screenVertXY = (cellVtx + camXY - vec2f(f32(cellMapGridSize) / 2)) * f32(cellPixelSize);
    let screenVertXY = iso * (cellVtx + camXY - vec2f(f32(cellMapGridSize) / 2)) * f32(cellPixelSize);

    let ndc = (screenVertXY / vec2f(logicalTargetWidth, logicalTargetHeight)) * vec2f(2.0, -2.0);

    // Dimensions of the tile sheet in pixels.
    let sheetPixelSize = textureDimensions(tileSheets).x;
    // Number of pixels along each axis of a tile.
    // Sheets should be a multiple of tile size.
    let tilePixelSize = sheetPixelSize / sheetGridSize;
    // Number of cells along each axis of a tile.
    let tileCellSize = tilePixelSize / cellPixelSize;

    // A single tile can be >= the size of a cell, but should be a multiple of the cell size.
    // A single sheet/atlas contains 1 or more tiles, but should be square,
    // and should be a multple of the tile pixel size.

    // If we imagine the map is not only a grid of cells, but also
    // a grid of tiles, then we calculate the index of the tile in the tile map.

    // Coords of the tile containing the cell in tile map space.
    let mapTileXY = mapCellXY / tileCellSize;
    let tileColumnCount = u32(ceil(f32(cellMapGridSize) / f32(tileCellSize)));
    // Index of the tile in the tile map.
    let mapTileIndex = mapTileXY.y * tileColumnCount + mapTileXY.x;

    // Choose a deterministically random tile within the sheet.
    let sheetTileIndex = hash(mapTileIndex) % (sheetGridSize * sheetGridSize);

    // Coords of the cell in tile space (within the tile).
    // IOW, if the sheet contains 4x4 tiles, then a cell's position within
    // the tile would be (0,0) for the top-left cell, (1,0) for the cell to its right, etc.
    let tileCellXY = vec2f(f32(mapCellXY.x % tileCellSize), f32(mapCellXY.y % tileCellSize));
    // Position of the vertex (corner of the cell) within the tile.
    var tileVertXY = tileCellXY + cellVtx;

    // Coords of the tile in sheet space (within the sheet).
    let sheetTileXY = vec2f(f32(sheetTileIndex % sheetGridSize), f32(sheetTileIndex / sheetGridSize));

    // UV coordinates of the vertex within the tile.
    let tileVertUV = tileVertXY / vec2f(f32(tileCellSize));

    return VSOut(
        vec4f(ndc, 1.0, 1.0), // pos
        (sheetTileXY + tileVertUV) / vec2f(f32(sheetGridSize)), // uv
        cellVtx, // gridUV
        layer % textureNumLayers(tileSheets), // texLayer
    );
}

fn linear_to_srgb(linear: vec4f) -> vec4f
{
    let lo = vec4f(linear.rgb * 12.92, linear.a);
    let hi = vec4f(1.055 * pow(max(linear.rgb, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055, linear.a);

    return select(hi, lo, vec4(linear.rgb <= vec3f(0.0031308), true));
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4f
{
    let thickness = fwidth(i.gridUV);
    let onGrid = i.gridUV.x < thickness.x || i.gridUV.y < thickness.y;
    var p = textureSample(tileSheets, smp, i.uv, i.texLayer);
    return select(p, vec4f(1.0, 0.0, 1.0, 1.0), onGrid);
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4f
{
    let thickness = fwidth(i.gridUV);
    let onGrid = i.gridUV.x < thickness.x || i.gridUV.y < thickness.y;
    var p = linear_to_srgb(textureSample(tileSheets, smp, i.uv, i.texLayer));
    return select(p, vec4f(1.0, 0.0, 1.0, 1.0), onGrid);
}
)";
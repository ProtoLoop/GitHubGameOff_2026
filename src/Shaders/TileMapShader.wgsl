constexpr const char* const kTileMapShaderCode = R"(
struct VSOut
{
    @builtin(position)  pos : vec4f,
    @location(0)        uv  : vec2f,
    @location(1)        origUV : vec2f,
    @location(2)        maskuv : vec2f,
    @location(3) @interpolate(flat) sheet : u32,
    @location(4) @interpolate(flat) cellOccupied : u32,
};

struct CellMapParams
{
    viewRow : f32, // Camera view row in world space
    viewCol : f32, // Camera view column in world space
};

@group(0) @binding(0) var tileSheets : texture_2d_array<f32>;
@group(0) @binding(1) var smp : sampler;
@group(0) @binding(2) var<uniform> cellMapParams : CellMapParams;
@group(0) @binding(3) var<storage, read> cellOccupancy : array<u32>;

override logicalTargetWidth: f32;   // Logical width of the render target in pixels
override logicalTargetHeight: f32;  // Logical height of the render target in pixels
override cellPixelSize: u32;        // Number of pixels along each axis of a cell
override sheetGridSize: u32;        // Number of tiles along each axis in the tile sheet
override cellMapGridSize: u32;      // Number of cells along each axis in the map
    
const vsout = array<VSOut, 6>
(
    VSOut(vec4f(-1.0, 1.0, 0.0, 1.0), vec2f(0.0, 0.0), vec2f(0.0, 0.0), vec2f(0.0, 0.0), 0, 0u),
    VSOut(vec4f(1.0, 1.0, 0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 0.0), 0, 0u),
    VSOut(vec4f(1.0, -1.0, 0.0, 1.0), vec2f(1.0, 1.0), vec2f(1.0, 1.0), vec2f(0.0, 0.0), 0, 0u),

    VSOut(vec4f(-1.0, 1.0, 0.0, 1.0), vec2f(0.0, 0.0), vec2f(0.0, 0.0), vec2f(0.0, 0.0), 0, 0u),
    VSOut(vec4f(1.0, -1.0, 0.0, 1.0), vec2f(1.0, 1.0), vec2f(1.0, 1.0), vec2f(0.0, 0.0), 0, 0u),
    VSOut(vec4f(-1.0, -1.0, 0.0, 1.0), vec2f(0.0, 1.0), vec2f(0.0, 1.0), vec2f(0.0, 0.0), 0, 0u)
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
    let logicalTargetSize = vec2f(logicalTargetWidth, logicalTargetHeight);

    let cellOccupied = (cellOccupancy[worldCellIndex / 32u] & (1u << (worldCellIndex % 32u))) != 0u;

    let cellsPerLayer = cellMapGridSize * cellMapGridSize;
    let layer = worldCellIndex / cellsPerLayer;
    let cell = worldCellIndex % cellsPerLayer;

    // World space cell coords
    let worldCellRow = cell / cellMapGridSize;
    let worldCellCol = cell % cellMapGridSize;

    var v = vsout[vidx];

    let world = v.origUV + vec2f(f32(worldCellCol), f32(worldCellRow));
    let cam = world - vec2f(cellMapParams.viewCol, cellMapParams.viewRow);
    let ndc = (iso * cam * f32(cellPixelSize) / logicalTargetSize) * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0);
    //let ndc = (cam * f32(cellPixelSize) / logicalTargetSize) * vec2f(2.0, -2.0);

    v.pos.xy = ndc;

    let sheetPixelSize = textureDimensions(tileSheets).x;
    // Number of pixels along each axis of a terrain tile.
    let tilePixelSize = sheetPixelSize / sheetGridSize;
    // Number of world cells covered along each axis of a terrain tile.
    let tileCellSize = tilePixelSize / cellPixelSize;

    // World space terrain tile coords.
    let worldTerrainRow = worldCellRow / tileCellSize;
    let worldTerrainCol = worldCellCol / tileCellSize;
    let terrainColumnCount = u32(ceil(f32(cellMapGridSize) / f32(tileCellSize)));
    let worldTerrainIndex = worldTerrainRow * terrainColumnCount + worldTerrainCol;

    // Choose a deterministically random tile index for this terrain tile.
    let hashVal = hash(worldTerrainIndex);
    let tileIndex = hashVal % (sheetGridSize * sheetGridSize);

    // Offset of the cell within its terrain tile.
    let cellWithinTile = vec2f(f32(worldCellCol % tileCellSize), f32(worldCellRow % tileCellSize));
    var terrainUV = (cellWithinTile + v.uv) / f32(tileCellSize);

    // Offset of the terrain tile within the tile sheet.
    let tileCol = tileIndex % sheetGridSize;
    let tileRow = tileIndex / sheetGridSize;
    var tileUV = vec2f(f32(tileCol), f32(tileRow));

    v.uv = (terrainUV + tileUV) / vec2f(f32(sheetGridSize));

    let sheetCount = textureNumLayers(tileSheets);
    v.sheet = layer % sheetCount;

    return v;
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
    let thickness = fwidth(i.origUV);
    let onGrid = i.origUV.x < thickness.x || i.origUV.y < thickness.y;
    var p = textureSample(tileSheets, smp, i.uv, i.sheet);
    return select(p, vec4f(1.0, 0.0, 1.0, 1.0), onGrid);
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4f
{
    let thickness = fwidth(i.origUV);
    let onGrid = i.origUV.x < thickness.x || i.origUV.y < thickness.y;
    var p = linear_to_srgb(textureSample(tileSheets, smp, i.uv, i.sheet));
    return select(p, vec4f(1.0, 0.0, 1.0, 1.0), onGrid);
}
)";
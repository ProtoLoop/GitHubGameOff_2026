const char* const kTileMapShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
    @location(1) @interpolate(flat)       sheet : u32,
};

struct CellMapParams
{
    // Logical size of the render target.
    logicalTargetSize : vec2<f32>,
    cellSize : f32,// Also the size of tiles in the tile sheet
    tileSheetRowCount : u32,
    tileSheetColCount : u32,
    cellMapRowCount : u32,
    cellMapColCount : u32,

    // Pad to 16 bytes for alignment with std140 layout rules.
    pad0: u32,
};

@group(0) @binding(0) var tileSheets : texture_2d_array<f32>;
@group(0) @binding(1) var smp : sampler;
@group(0) @binding(2) var<uniform> cellMapParams : CellMapParams;

const vsout = array<VSOut, 6>
(
    VSOut(vec4<f32>(-1.0, 1.0, 0.0, 1.0), vec2<f32>(0.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, 1.0, 0.0, 1.0), vec2<f32>(1.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, -1.0, 0.0, 1.0), vec2<f32>(1.0, 1.0), 0),

    VSOut(vec4<f32>(-1.0, 1.0, 0.0, 1.0), vec2<f32>(0.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, -1.0, 0.0, 1.0), vec2<f32>(1.0, 1.0), 0),
    VSOut(vec4<f32>(-1.0, -1.0, 0.0, 1.0), vec2<f32>(0.0, 1.0), 0)
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
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) cellIndex: u32) -> VSOut
{
    let screenRows = cellMapParams.logicalTargetSize.y / cellMapParams.cellSize;
    let screenCols = cellMapParams.logicalTargetSize.x / cellMapParams.cellSize;

    let cellRow = cellIndex / cellMapParams.cellMapColCount;
    let cellCol = cellIndex % cellMapParams.cellMapColCount;

    // Center of the cell in pixel coordinates.
    let centerPx = (vec2f(f32(cellCol), f32(cellRow)) + vec2f(0.5)) * cellMapParams.cellSize;

    // Center of the cell in normalized device coordinates.
    let centerNDC = (centerPx / (cellMapParams.logicalTargetSize / vec2<f32>(2.0, -2.0)))+ vec2f(-1.0, 1.0);

    // Half size of the cell in normalized device coordinates.  No division by 2 because each cell
    // spans from -1 to 1 (2 units) in local space NDC.
    let cellHalfSizeNDC = vec2f(f32(cellMapParams.cellSize)) / cellMapParams.logicalTargetSize;

    var v = vsout[vid];

    // Scale and translate the vertex position to the correct cell in NDC.
    v.pos = vec4<f32>(centerNDC + v.pos.xy * cellHalfSizeNDC, v.pos.z, v.pos.w);

    let hashVal = hash(cellIndex);
    let tileIndex = hashVal % (cellMapParams.tileSheetRowCount * cellMapParams.tileSheetColCount);
    let sheetCount = textureNumLayers(tileSheets);

    let tileCol = tileIndex % cellMapParams.tileSheetColCount;
    let tileRow = tileIndex / cellMapParams.tileSheetColCount;
    var tileUV = vec2<f32>(f32(tileCol), f32(tileRow));

    v.uv = (v.uv + tileUV) / vec2<f32>(f32(cellMapParams.tileSheetColCount), f32(cellMapParams.tileSheetRowCount));

    // hflip and vflip
    v.uv.x = select(v.uv.x, 1.0 - v.uv.x, (hashVal & 1u) != 0u);
    v.uv.y = select(v.uv.y, 1.0 - v.uv.y, (hashVal & 2u) != 0u);

    v.sheet = (cellIndex / (cellMapParams.tileSheetRowCount * cellMapParams.tileSheetColCount)) % sheetCount;

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
fn fs_main_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    return textureSample(tileSheets, smp, i.uv, i.sheet);
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    return linear_to_srgb(textureSample(tileSheets, smp, i.uv, i.sheet));
}
)";
constexpr const char* const kSpriteShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
    @location(1) @interpolate(flat)       sheet : u32,
};

struct SpriteState
{
    frame: u32,
    dir: u32,
    sheet: u32,
};

@group(0) @binding(0) var animSheets : texture_2d_array<f32>;
@group(0) @binding(1) var smp : sampler;
@group(0) @binding(2) var<storage, read> spriteStates : array<SpriteState>;

override logicalTargetWidth: f32 = 800;     // Logical width of the render target in pixels
override logicalTargetHeight: f32 = 600;    // Logical height of the render target in pixels
override cellPixelSize: u32 = 64;           // Number of pixels along each axis of a cell
override sheetGridSize: u32 = 1;            // Number of tiles along each axis in the tile sheet
override cellMapGridSize: u32 = 128;        // Number of cells along each axis in the map

// Number of rows and columns in each tile sheet.
const sheetRowCount: u32 = 4u;
const sheetColCount: u32 = 6u;
const sheetColsRows = vec2<f32>(f32(sheetColCount), f32(sheetRowCount));

const vsout = array<VSOut, 6>
(
    VSOut(vec4<f32>(-1.0, 1.0, 0.0, 1.0), vec2<f32>(0.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, 1.0, 0.0, 1.0), vec2<f32>(1.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, -1.0, 0.0, 1.0), vec2<f32>(1.0, 1.0), 0),

    VSOut(vec4<f32>(-1.0, 1.0, 0.0, 1.0), vec2<f32>(0.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, -1.0, 0.0, 1.0), vec2<f32>(1.0, 1.0), 0),
    VSOut(vec4<f32>(-1.0, -1.0, 0.0, 1.0), vec2<f32>(0.0, 1.0), 0)
);

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) spriteId: u32) -> VSOut
{
    let texDims = vec2<f32>(textureDimensions(animSheets, 0).xy);
    let spriteSize = texDims / sheetColsRows;
    var v = vsout[vid];

    let state = spriteStates[spriteId];

    let logicalTargetSize = vec2<f32>(logicalTargetWidth, logicalTargetHeight);
    v.pos = vec4<f32>(v.pos.xy * (spriteSize / logicalTargetSize), v.pos.zw);

    let frame = f32(state.frame % sheetColCount);
    let dir = f32(state.dir % sheetRowCount);

    v.uv = (v.uv + vec2<f32>(frame, dir)) / sheetColsRows;

    v.sheet = state.sheet;

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
    return textureSample(animSheets, smp, i.uv, i.sheet);
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    return linear_to_srgb(textureSample(animSheets, smp, i.uv, i.sheet));
}
)";
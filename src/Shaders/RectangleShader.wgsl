constexpr const char* const kRectangleShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(1)       brectUV : vec2f, // Used to draw bounding rectangle
    @location(2)       color : vec4<f32>,
};

struct CellMapParams
{
    proj : mat2x2f,     // projection matrix (isometric, orthographic, etc)
    lightDir : vec3f,   // Direction of the light in world space
    cellMapCellsPerSide : u32, // Number of cells along each axis in the map
    viewXY : vec2f,     // Camera view position in world space
    pad0 : u32,
    pad1 : u32,
};

struct Rectangle
{
    xywh : vec4f,       // x, y, width, height in pixels
    color : vec4f,      // linear RGBA outline color
    cellXY : vec2f,     // Anchor point of the rectangle in cell coordinates (x, y) - where the pivot point lies in map space
    pad0: u32,
    pad1: u32,
};

@group(0) @binding(0) var<storage, read> rectangles : array<Rectangle>;
@group(0) @binding(1) var<uniform> cellMapParams : CellMapParams;

override logicalTargetWidth: f32;   // Logical width of the render target in pixels
override logicalTargetHeight: f32;  // Logical height of the render target in pixels
override cellPixelsPerSide: u32;    // Number of pixels along each axis of a cell
override sheetTilesPerSide: u32;    // Number of tiles along each axis in the tile sheet
override shadowPass: u32;           // Indicates whether the current pass is a shadow pass (1 for shadow, 0 otherwise)
    
const rectVerts = array<vec2f, 6>(
    vec2f(0, 0),
    vec2f(1, 0),
    vec2f(1, 1),

    vec2f(0, 0),
    vec2f(1, 1),
    vec2f(0, 1)
);

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) rectId: u32) -> VSOut
{
    let logicalTargetSize = vec2f(logicalTargetWidth, logicalTargetHeight);

    let rect = rectangles[rectId];

    // vec2f(1.0, -1.0) - flip Y from sprite image coordinates (Y down) to screen space coordinates (Y up)
    let offset = (rect.xywh.xy + rectVerts[vid] * rect.xywh.zw) * vec2f(1.0, -1.0);

    // Round to the nearest pixel in camera space.
    // This duplicates the rounding logic used for tiles to ensure pixel-perfect alignment
    // to minimize sub-pixel jitter.
    let cameraPixels = round(cellMapParams.proj * cellMapParams.viewXY * f32(cellPixelsPerSide));

    // Projected anchor position in logical pixels, before camera subtraction.
    let worldPixels = cellMapParams.proj * rect.cellXY * f32(cellPixelsPerSide);

    // Position of the vertex (corner of the rect) in screen space.
    let screenVertXY = worldPixels - cameraPixels + offset;
    
    let ndc = (screenVertXY / logicalTargetSize) * vec2f(2.0, 2.0);

    return VSOut(
        vec4f(ndc, 1.0, 1.0), // pos
        rectVerts[vid],  // brectUV
        rect.color,   // color
    );
}

fn linear_to_srgb(linear: vec4f) -> vec4f
{
    let lo = vec4f(linear.rgb * 12.92, linear.a);
    let hi = vec4f(1.055 * pow(max(linear.rgb, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055, linear.a);

    return select(hi, lo, vec4(linear.rgb <= vec3f(0.0031308), true));
}

// Draws the bounding rectangle based on its UV coordinates.
fn drawBoundingRect(rectUV: vec2f, color: vec4f) -> vec4f
{
    let thickness = fwidth(rectUV);
    let edgeDistance = min(rectUV, vec2f(1.0) - rectUV);

    if (!any(edgeDistance < thickness))
    {
        discard;
    }
    return color;
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    var p = drawBoundingRect(i.brectUV, i.color);
    return p;
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    var p = drawBoundingRect(i.brectUV, i.color);
    return linear_to_srgb(p);
}
)";
constexpr const char* const kSpriteShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
    @location(1)       brectUV : vec2f, // Used to draw bounding rectangle
    @location(2)       tint : vec4<f32>,
    @location(3)       normPivot : vec2<f32>,   // Normalized pivot within the sprite frame (0,0) top-left, (1,1) bottom-right.
};

struct CellMapParams
{
    proj : mat2x2f, // projection matrix (isometric, orthographic, etc)
    viewXY : vec2f, // Camera view position in world space
    cellMapCellsPerSide : u32, // Number of cells along each axis in the map
    pad1 : u32,
};

struct SpriteState
{
    cellXY: vec2f,  // World-cell position of the sprite’s pivot.
    pivot: vec2f,   // Pivot point within the sprite frame in frame local pixels.  0,0 is top-left.
    tint : vec4f,   // Linear RGBA tint.
    srcRect: vec4f, // Source rectangle in texture pixels, before scaling.
    scale : f32,    // Uniform scale applied to image offsets and dimensions.
    pad0: u32,
    pad1: u32,
    pad2: u32,
};

@group(0) @binding(0) var<storage, read> spriteStates : array<SpriteState>;
@group(0) @binding(1) var<uniform> cellMapParams : CellMapParams;

@group(1) @binding(0) var texture : texture_2d_array<f32>;
@group(1) @binding(1) var smp : sampler;

override logicalTargetWidth: f32;   // Logical width of the render target in pixels
override logicalTargetHeight: f32;  // Logical height of the render target in pixels
override cellPixelsPerSide: u32;    // Number of pixels along each axis of a cell
override sheetTilesPerSide: u32;    // Number of tiles along each axis in the tile sheet
    
const spriteVerts = array<vec2f, 6>(
    vec2f(0, 0),
    vec2f(1, 0),
    vec2f(1, 1),

    vec2f(0, 0),
    vec2f(1, 1),
    vec2f(0, 1)
);

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) spriteId: u32) -> VSOut
{
    let logicalTargetSize = vec2f(logicalTargetWidth, logicalTargetHeight);

    let sprite = spriteStates[spriteId];

    // Apply the pivot in Y-down sprite coordinates, then convert the offset to Y-up screen space.
    let spriteVtx = ((spriteVerts[vid] * sprite.srcRect.zw) - sprite.pivot) * vec2f(1.0, -1.0);

    // The camera is rounded in projected logical pixels.
    // Matching this in the tile map shader keeps camera movement consistent across shaders
    // and avoid sub-pixel jitter.
    let cameraPixels = round(cellMapParams.proj * cellMapParams.viewXY * f32(cellPixelsPerSide));

    // Projected anchor position in logical pixels, before camera subtraction.
    let worldPixels = cellMapParams.proj * sprite.cellXY * f32(cellPixelsPerSide);

    // Position of the vertex (corner of the sprite) in screen space.
    let screenVertXY = worldPixels - cameraPixels + spriteVtx * sprite.scale;
    
    let ndc = (screenVertXY / logicalTargetSize) * vec2f(2.0, 2.0);

    let texDims = vec2f(textureDimensions(texture, 0).xy);
    let uvOffset = sprite.srcRect.xy / texDims;
    let uvDims = sprite.srcRect.zw / texDims;

    let uv = uvOffset + (spriteVerts[vid] * uvDims);

    return VSOut(
        vec4f(ndc, 1.0, 1.0), // pos
        uv,             // uv
        spriteVerts[vid],  // brectUV
        sprite.tint,   // tint
        sprite.pivot / sprite.srcRect.zw    // normalized pivot
    );
}

fn linear_to_srgb(linear: vec4f) -> vec4f
{
    let lo = vec4f(linear.rgb * 12.92, linear.a);
    let hi = vec4f(1.055 * pow(max(linear.rgb, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055, linear.a);

    return select(hi, lo, vec4(linear.rgb <= vec3f(0.0031308), true));
}

// Draws the bounding rectangle for a sprite based on its UV coordinates.
fn drawBoundingRect(gridUV: vec2f, color: vec4f) -> vec4f
{
    let thickness = fwidth(gridUV);
    let edgeDistance = min(gridUV, vec2f(1.0) - gridUV);
    return select(color, vec4f(1.0, 1.0, 0.0, 1.0), any(edgeDistance < thickness));
}

// Draws the pivot cross for a sprite. The pivot is specified in sprite-local normalized coordinates.
fn drawPivot(i: VSOut, color: vec4f) -> vec4f
{
    let pixelUV = max(fwidth(i.brectUV), vec2f(0.000001));
    let distance = abs(i.brectUV - i.normPivot) / pixelUV;

    let onCross =
        (distance.x < 6.0 && distance.y < 1.0) ||
        (distance.y < 6.0 && distance.x < 1.0);

    return select(color, vec4f(1.0, 0.0, 1.0, 1.0), onCross);
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    var p = textureSample(texture, smp, i.uv, 0) * i.tint;
    //p = drawBoundingRect(i.brectUV, p);
    p = drawPivot(i, p);
    return p;
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    var p = textureSample(texture, smp, i.uv, 0) * i.tint;
    //p = drawBoundingRect(i.brectUV, p);
    p = drawPivot(i, p);
    return linear_to_srgb(p);
}
)";
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
    proj : mat2x2f,     // projection matrix (isometric, orthographic, etc)
    lightDir : vec3f,   // Direction of the light in world space
    cellMapCellsPerSide : u32, // Number of cells along each axis in the map
    viewXY : vec2f,     // Camera view position in world space
    pad0 : u32,
    pad1 : u32,
};

struct SpriteState
{
    cellXY: vec2f,  // World-cell position of the sprite’s pivot.
    pivot: vec2f,   // Pivot point within the sprite frame in frame local pixels.  0,0 is top-left.  Used to debug draw the pivot.
    tint : vec4f,   // Linear RGBA tint.
    srcRect: vec4f, // Source rectangle in texture pixels.
    dstRect: vec4f, // Destination rectangle in screen pixels.
};

@group(0) @binding(0) var<storage, read> spriteStates : array<SpriteState>;
@group(0) @binding(1) var<uniform> cellMapParams : CellMapParams;

@group(1) @binding(0) var texture : texture_2d_array<f32>;
@group(1) @binding(1) var smp : sampler;

override logicalTargetWidth: f32;   // Logical width of the render target in pixels
override logicalTargetHeight: f32;  // Logical height of the render target in pixels
override cellPixelsPerSide: u32;    // Number of pixels along each axis of a cell
override sheetTilesPerSide: u32;    // Number of tiles along each axis in the tile sheet
override shadowPass: u32;           // Indicates whether the current pass is a shadow pass (1 for shadow, 0 otherwise)
    
const spriteVerts = array<vec2f, 6>(
    vec2f(0, 0),
    vec2f(1, 0),
    vec2f(1, 1),

    vec2f(0, 0),
    vec2f(1, 1),
    vec2f(0, 1)
);

const shadowColor = vec4f(0,0,0, 0.5);

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) spriteId: u32) -> VSOut
{
    let logicalTargetSize = vec2f(logicalTargetWidth, logicalTargetHeight);

    let sprite = spriteStates[spriteId];

    // Calculate the vertex position of the dest rectangle in screen space.
    // Flip the Y coordinate to convert from Y-down sprite space to Y-up screen space.
    let spriteVtx = (spriteVerts[vid] * sprite.dstRect.zw + sprite.dstRect.xy) * vec2f(1.0, -1.0);

    // The camera is rounded in projected logical pixels.
    // Matching this in the tile map shader keeps camera movement consistent across shaders
    // and avoid sub-pixel jitter.
    let cameraPixels = round(cellMapParams.proj * cellMapParams.viewXY * f32(cellPixelsPerSide));

    // Projected anchor position in logical pixels, before camera subtraction.
    let worldPixels = cellMapParams.proj * sprite.cellXY * f32(cellPixelsPerSide);

    let basePx = worldPixels - cameraPixels;

    var ndc: vec2f;

    if(shadowPass == 0u)
    {
        // Position of the vertex (corner of the sprite) in screen space.
        let screenVertXY = basePx + spriteVtx;
        ndc = (screenVertXY / logicalTargetSize) * vec2f(2.0, 2.0);

    }
    else
    {
        let groundOffsetPerWorldHeight = cellMapParams.lightDir.xy / -cellMapParams.lightDir.z;
        let shadowVertXY = basePx + vec2(spriteVtx.x, 0) + spriteVtx.y * (cellMapParams.proj * groundOffsetPerWorldHeight);
        ndc = (shadowVertXY / logicalTargetSize) * vec2f(2.0, 2.0);
    }
    
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
    var p = textureSample(texture, smp, i.uv, 0);

    if(shadowPass > 0u)
    {
        p = vec4f(shadowColor.rgb, shadowColor.a * p.a * i.tint.a);
    }
    else
    {
        p *= i.tint;
    }

    //p = drawPivot(i, p);

    return p;
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    var p = textureSample(texture, smp, i.uv, 0);
    
    if(shadowPass > 0u)
    {
        p = vec4f(shadowColor.rgb, shadowColor.a * p.a * i.tint.a);
    }
    else
    {
        p *= i.tint;
    }

    //p = drawPivot(i, p);

    return linear_to_srgb(p);
}
)";
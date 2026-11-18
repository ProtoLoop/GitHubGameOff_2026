constexpr const char* const kSpriteShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
    @location(1)       outlineUV : vec2f, // Used to draw outlines
    @location(2)       color : vec4<f32>,
};

struct CellMapParams
{
    viewXY : vec2f, // Camera view position in world space
    cellMapCellsPerSide : u32, // Number of cells along each axis in the map
    pad1 : u32,
};

struct SpriteState
{
    mapXY: vec2f,   // Map space cell coordinates of the sprite.
    pivot: vec2f,   // Pivot point of the sprite.
    color : vec4f,  // Color tint for the sprite
    srcRect: vec4f, // Source rectangle in the texture (x, y, w, h)
    scale : f32,    // Scale factor for the sprite
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

const spriteUV = array<vec2f, 6>(
    vec2f(0.0, 0.0),
    vec2f(1.0, 0.0),
    vec2f(1.0, 1.0),

    vec2f(0.0, 0.0),
    vec2f(1.0, 1.0),
    vec2f(0.0, 1.0)
);

// Isometric transformation matrix.
const iso = mat2x2f(vec2f(0.5, -0.25), vec2f(0.5, 0.25));

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) spriteId: u32) -> VSOut
{
    let logicalTargetSize = vec2f(logicalTargetWidth, logicalTargetHeight);

    let sprite = spriteStates[spriteId];

    // Apply the pivot in Y-down sprite coordinates, then convert the offset to Y-up screen space.
    let spriteVtx = (spriteVerts[vid] - sprite.pivot) * vec2f(1.0, -1.0);

    // Round to the nearest pixel in camera space.
    // This duplicates the rounding logic used for tiles to ensure pixel-perfect alignment.
    let cameraPixels = round(iso * cellMapParams.viewXY * f32(cellPixelsPerSide));
    // Coords of the sprite in world space (in pixels).
    let worldPixels = iso * sprite.mapXY * f32(cellPixelsPerSide);
    // Position of the vertex (corner of the sprite) in screen space.
    let screenVertXY = worldPixels - cameraPixels + spriteVtx * sprite.srcRect.zw * sprite.scale;
    let ndc = (screenVertXY / logicalTargetSize) * vec2f(2.0, 2.0);

    let texDims = vec2f(textureDimensions(texture, 0).xy);
    let uvOffset = sprite.srcRect.xy / texDims;
    let uvDims = sprite.srcRect.zw / texDims;

    let uv = uvOffset + (spriteUV[vid] * uvDims);

    return VSOut(
        vec4f(ndc, 1.0, 1.0), // pos
        uv,             // uv
        spriteUV[vid],  // outlineUV
        sprite.color    // color
    );
}

fn linear_to_srgb(linear: vec4f) -> vec4f
{
    let lo = vec4f(linear.rgb * 12.92, linear.a);
    let hi = vec4f(1.055 * pow(max(linear.rgb, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055, linear.a);

    return select(hi, lo, vec4(linear.rgb <= vec3f(0.0031308), true));
}

// Returns true if a fragment is on the grid lines based on its UV coordinates.
// Used to draw sprite bounding boxes.
fn isOnGrid(gridUV: vec2f) -> bool
{
    let thickness = fwidth(gridUV);
    return gridUV.x < thickness.x || gridUV.y < thickness.y;
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    let p = textureSample(texture, smp, i.uv, 0) * i.color;
    //return select(p, vec4f(1.0, 1.0, 0.0, 1.0), isOnGrid(i.outlineUV));
    return p;
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    let p = linear_to_srgb(textureSample(texture, smp, i.uv, 0) * i.color);
    //return select(p, vec4f(1.0, 1.0, 0.0, 1.0), isOnGrid(i.outlineUV));
    return p;
}
)";
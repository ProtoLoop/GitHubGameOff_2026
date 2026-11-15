#include "Sprite.h"

#include "Angle.h"
#include "Renderer.h"

/// Sprite

Sprite::Sprite(wgpu::BindGroup bindGroup, const Rect& rect, const uint32_t layer)
    : m_TextureBindGroup(std::move(bindGroup)),
      m_Rect(rect),
      m_Layer(layer)
{
}

/// SpriteAnimLoader

SpriteAnimLoader::SpriteAnimLoader(const ResourceName& name,
    TextureLoader&& textureLoader,
    const Dimension2d& dim,
    Renderer* renderer)
    : m_Name(name),
      m_TextureLoader(std::move(textureLoader)),
      m_Dim(dim),
      m_Renderer(renderer)
{
}

Result<SpriteAnimLoader>
SpriteAnimLoader::Load(
    const SpriteAnimDef& animDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu)
{
    GG_CHECKB(!animDef.Textures.empty(), "Animation textures must not be empty");
    GG_CHECKB(animDef.Dim.w > 0 && animDef.Dim.h > 0,
        "Invalid animation dimensions: {}x{}",
        animDef.Dim.w,
        animDef.Dim.h);

    auto texLoadResult = TextureLoader::Load(animDef.Textures, fileFetcher, webGpu, "Anim");
    GG_CHECKB(texLoadResult, "Failed to start anim texture load");

    return SpriteAnimLoader(animDef.Name, std::move(*texLoadResult), animDef.Dim, &renderer);
}

const ResourceName&
SpriteAnimLoader::GetName() const
{
    return m_Name;
}

bool
SpriteAnimLoader::IsPending() const
{
    return m_TextureLoader.IsPending();
}

Result<>
SpriteAnimLoader::Update()
{
    return m_TextureLoader.Update();
}

Result<SpriteAnim>
SpriteAnimLoader::Take()
{
    GG_CHECKB(!IsPending(), "Sprite animation loader is still pending");
    GG_CHECKB(!m_Taken, "Sprite animation has already been taken");

    m_Taken = true;

    auto texResult = m_TextureLoader.Take();
    GG_CHECKB(texResult, "Failed to load anim texture");

    wgpu::Texture texture = *texResult;
    const uint32_t texW = texture.GetWidth();
    const uint32_t texH = texture.GetHeight();

    GG_CHECKB(texW % m_Dim.w == 0 && texH % m_Dim.h == 0,
        "Texture dimensions must be divisible by sprite dimensions: tex({},{}) sprite({},{})",
        texW,
        texH,
        m_Dim.w,
        m_Dim.h);

    const uint32_t spriteRows = texH / m_Dim.h;
    const uint32_t spriteCols = texW / m_Dim.w;
    const uint32_t framesPerDir = spriteRows * spriteCols;

    // Each layer in the texture corresponds to a direction.
    const uint32_t directionCount = texResult->GetDepthOrArrayLayers();
    const uint32_t spriteCount = directionCount * framesPerDir;

    GG_CHECKB(directionCount > 0, "Sprite animation has no directions");

    BoundedVector<Sprite> sprites(spriteCount);

    auto bgResult = m_Renderer->CreateSpriteTextureBindGroup(texture);
    GG_CHECKB(bgResult, "Failed to create bind group for sprite texture");

    for(uint32_t layer = 0; layer < directionCount; ++layer)
    {
        for(uint32_t row = 0; row < spriteRows; ++row)
        {
            for(uint32_t col = 0; col < spriteCols; ++col)
            {
                const Rect rect //
                    {
                        .x = col * m_Dim.w,
                        .y = row * m_Dim.h,
                        .w = m_Dim.w,
                        .h = m_Dim.h,
                    };

                GG_CHECKB(sprites.push_back(Sprite(*bgResult, rect, layer)),
                    "Failed to add sprite");
            }
        }
    }

    return SpriteAnim(m_Name, std::move(texture), std::move(sprites), framesPerDir);
}

/// SpriteAnim

SpriteAnim::SpriteAnim(const ResourceName& Name,
    wgpu::Texture&& m_Texture,
    BoundedVector<Sprite>&& m_Sprites,
    const uint32_t m_FramesPerDirection)
    : Name(Name),
      m_Texture(std::move(m_Texture)),
      m_Sprites(std::move(m_Sprites)),
      m_FramesPerDirection(m_FramesPerDirection)
{
}

const ResourceName&
SpriteAnim::GetName() const
{
    return Name;
}

Result<SpriteAnim::Instance>
SpriteAnim::CreateInstance() const
{
    return Instance(*this);
}

const Sprite&
SpriteAnim::GetFrame(const float phase, const Angle& angle) const
{
    const uint32_t directionCount = m_Texture.GetDepthOrArrayLayers();

    const float fdir = angle.ToTurns() * static_cast<float>(directionCount);
    const size_t direction = static_cast<size_t>(std::round(fdir)) % directionCount;
    const size_t framesPerDirection = m_FramesPerDirection;
    const float normalizedPhase = phase - std::floor(phase);
    const size_t frame =
        static_cast<size_t>(normalizedPhase * static_cast<float>(framesPerDirection))
        % framesPerDirection;

    const size_t index = (direction * framesPerDirection) + frame;

    return *m_Sprites.TryGet(index);
}

/// SpriteAnim::Instance

SpriteAnim::Instance::Instance(const SpriteAnim& anim)
    : m_Anim(&anim)
{
}

const ResourceName&
SpriteAnim::Instance::GetName() const
{
    return m_Anim->GetName();
}

void
SpriteAnim::Instance::Tick(const float deltaTime)
{
    SetPhase(m_Phase + (deltaTime / m_Period));
}

void
SpriteAnim::Instance::SetPeriod(const float period)
{
    constexpr float minPeriod = 1e-6f; // Minimum allowed animation period
    if(GG_VERIFYB(period > minPeriod, "Animation period must be positive"))
    {
        m_Period = period;
    }
}

void
SpriteAnim::Instance::SetPhase(const float phase)
{
    m_Phase = phase - std::floor(phase);
}

void
SpriteAnim::Instance::SetAngle(const Angle& angle)
{
    m_Angle = angle;
}

const Angle&
SpriteAnim::Instance::GetAngle() const
{
    return m_Angle;
}

const Sprite&
SpriteAnim::Instance::GetFrame() const
{
    return m_Anim->GetFrame(m_Phase, m_Angle);
}
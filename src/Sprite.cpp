#include "Sprite.h"

#include "Angle.h"
#include "Defer.h"
#include "Renderer.h"

/// Sprite

Sprite::Sprite(wgpu::BindGroup bindGroup, const Rect& rect, const uint32_t layer)
    : m_TextureBindGroup(std::move(bindGroup)),
      m_Rect(rect),
      m_Layer(layer)
{
}

/// SpriteAnim

SpriteAnim::SpriteAnim(LoadingState&& state)
    : m_State(std::move(state))
{
}

Result<SpriteAnim>
SpriteAnim::Load(
    const SpriteAnimDef& animDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu)
{
    GG_CHECKB(!animDef.Textures.empty(), "Animation textures must not be empty");
    GG_CHECKB(animDef.Dim.w > 0 && animDef.Dim.h > 0,
        "Invalid animation dimensions: {}x{}",
        animDef.Dim.w,
        animDef.Dim.h);

    auto texLoadResult = TextureLoader::Load(animDef.Textures, fileFetcher, webGpu, "Anim");
    GG_CHECKB(texLoadResult, "Failed to start anim texture load");

    return SpriteAnim(LoadingState //
        {
            .Name = animDef.Name,
            .m_TextureLoader = std::move(*texLoadResult),
            .m_Dim = animDef.Dim,
            .m_Renderer = &renderer,
        });
}

bool
SpriteAnim::IsPending() const
{
    return std::holds_alternative<LoadingState>(m_State);
}

Result<>
SpriteAnim::Update()
{
    if(std::holds_alternative<LoadingState>(m_State))
    {
        const LoadingState& state = std::get<LoadingState>(m_State);

        if(state.m_TextureLoader.IsPending())
        {
            return Result<>::Ok;
        }

        auto onFailed = GG_DEFER
        {
            if(std::holds_alternative<LoadingState>(m_State))
            {
                m_State = FailedState{};
            }
        };

        auto buildResult = BuildSprites();
        GG_CHECKB(buildResult, "Failed to build sprites");
        m_State = std::move(*buildResult);
    }

    return Result<>::Ok;
}

const ResourceName&
SpriteAnim::GetName() const
{
    if(std::holds_alternative<LoadingState>(m_State))
    {
        return std::get<LoadingState>(m_State).Name;
    }
    else if(std::holds_alternative<LoadedState>(m_State))
    {
        return std::get<LoadedState>(m_State).Name;
    }
    else
    {
        GG_ASSERT(false, "Sprite animation is in an invalid state");

        static constexpr ResourceName kInvalidName{"*** Invalid SpriteAnim ***"};
        
        return kInvalidName;
    }
}

Angle
SpriteAnim::SnapAngle(const Angle& angle) const
{
    if(!GG_VERIFYB(std::holds_alternative<LoadedState>(m_State), "Sprite animation is not loaded"))
    {
        return Angle(0.0f);
    }

    const LoadedState& state = std::get<LoadedState>(m_State);

    const uint32_t directionCount = state.m_TileSetTexture.GetDepthOrArrayLayers();
    if(!GG_VERIFYB(directionCount > 0, "Sprite animation has no directions"))
    {
        return Angle(0.0f);
    }

    const float fdir = angle.ToTurns() * static_cast<float>(directionCount);
    const uint32_t dir = static_cast<uint32_t>(std::round(fdir)) % directionCount;
    return Angle(static_cast<float>(dir) / static_cast<float>(directionCount));
}

Result<const Sprite*>
SpriteAnim::GetSprite(const float phase, const Angle& angle) const
{
    GG_CHECKB(std::holds_alternative<LoadedState>(m_State), "Sprite animation is not loaded");

    const LoadedState& state = std::get<LoadedState>(m_State);

    const uint32_t directionCount = state.m_TileSetTexture.GetDepthOrArrayLayers();
    GG_CHECKB(directionCount > 0, "Sprite animation has no directions");

    const float fdir = angle.ToTurns() * static_cast<float>(directionCount);
    const uint32_t dir = static_cast<uint32_t>(std::round(fdir)) % directionCount;

    const float normalizedPhase = phase - std::floor(phase);
    const size_t frame =
        static_cast<size_t>(normalizedPhase * static_cast<float>(state.m_FramesPerDirection));
    const size_t normalizedFrame = frame % state.m_FramesPerDirection;

    const size_t index = static_cast<size_t>(dir * state.m_FramesPerDirection) + normalizedFrame;
    const Sprite* sprite = state.m_Sprites.TryGet(index);
    GG_CHECKB(sprite, "Invalid sprite index: {}", index);

    return sprite;
}

// private:

Result<SpriteAnim::LoadedState>
SpriteAnim::BuildSprites()
{
    GG_CHECKB(std::holds_alternative<LoadingState>(m_State),
        "Sprite animation is not in loading state");
    LoadingState& state = std::get<LoadingState>(m_State);

    auto texResult = state.m_TextureLoader.Take();
    GG_CHECKB(texResult, "Failed to load anim texture");

    wgpu::Texture texture = *texResult;
    const uint32_t texW = texture.GetWidth();
    const uint32_t texH = texture.GetHeight();
    const Dimension2d spriteDim = state.m_Dim;

    GG_CHECKB(texW % spriteDim.w == 0 && texH % spriteDim.h == 0,
        "Texture dimensions must be divisible by sprite dimensions: tex({},{}) sprite({},{})",
        texW,
        texH,
        spriteDim.w,
        spriteDim.h);

    const uint32_t spriteRows = texH / spriteDim.h;
    const uint32_t spriteCols = texW / spriteDim.w;
    const uint32_t framesPerDir = spriteRows * spriteCols;
    // Each layer in the texture corresponds to a direction.
    const uint32_t directionCount = texResult->GetDepthOrArrayLayers();
    const uint32_t spriteCount = directionCount * framesPerDir;

    BoundedVector<Sprite> sprites(spriteCount);

    auto bgResult = state.m_Renderer->CreateSpriteTextureBindGroup(texture);
    GG_CHECKB(bgResult, "Failed to create bind group for sprite texture");

    for(uint32_t layer = 0; layer < directionCount; ++layer)
    {
        for(uint32_t row = 0; row < spriteRows; ++row)
        {
            for(uint32_t col = 0; col < spriteCols; ++col)
            {
                const Rect rect //
                    {
                        .x = col * spriteDim.w,
                        .y = row * spriteDim.h,
                        .w = spriteDim.w,
                        .h = spriteDim.h,
                    };

                GG_CHECKB(sprites.push_back(Sprite(*bgResult, rect, layer)),
                    "Failed to add sprite");
            }
        }
    }

    return LoadedState //
        {
            .Name = std::move(state.Name),
            .m_TileSetTexture = std::move(texture),
            .m_Sprites = std::move(sprites),
            .m_FramesPerDirection = framesPerDir,
        };
}
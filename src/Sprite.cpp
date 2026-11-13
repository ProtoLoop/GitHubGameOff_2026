#include "Sprite.h"

#include "Angle.h"
#include "Defer.h"

/// Sprite

Sprite::Sprite(wgpu::Texture texture, const Rect& rect, const uint32_t layer)
    : m_Texture(std::move(texture)),
      m_Rect(rect),
      m_Layer(layer)
{
}

/// SpriteAnim

Result<SpriteAnim>
SpriteAnim::Create(const SpriteAnimDef& animDef, FileFetcher& fileFetcher, WebGpu& webGpu)
{
    GG_CHECKB(!animDef.Textures.empty(), "Animation textures must not be empty");
    GG_CHECKB(animDef.Dim.w > 0 && animDef.Dim.h > 0,
        "Invalid animation dimensions: {}x{}",
        animDef.Dim.w,
        animDef.Dim.h);

    auto texLoadResult = TextureLoader::Load(animDef.Textures, fileFetcher, webGpu, "Anim");
    GG_CHECKB(texLoadResult, "Failed to start anim texture load");

    return SpriteAnim(std::move(*texLoadResult), animDef.Dim);
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

        auto onFailed = GG_DEFER
        {
            m_State = std::monostate{};
        };

        if(!state.m_TextureLoader.IsPending())
        {
            auto buildResult = BuildSprites();
            GG_CHECKB(buildResult, "Failed to build sprites");
            m_State = std::move(*buildResult);
        }

        onFailed.cancel();
    }

    return Result<>::Success();
}

SpriteAnim::SpriteAnim(TextureLoader textureLoader, const Dimension2d& dim)
    : m_State(LoadingState //
          {
              .m_TextureLoader = std::move(textureLoader),
              .m_Dim = dim,
          })
{
}

Result<Angle>
SpriteAnim::SnapAngle(const Angle& angle) const
{
    GG_CHECKB(std::holds_alternative<LoadedState>(m_State), "Sprite animation is not loaded");

    const LoadedState& state = std::get<LoadedState>(m_State);
    const float fdir = angle.ToTurns() * static_cast<float>(state.m_DirectionCount);
    const uint32_t dir = static_cast<uint32_t>(std::round(fdir)) % state.m_DirectionCount;
    return Angle(static_cast<float>(dir) / static_cast<float>(state.m_DirectionCount));
}

Result<const Sprite*>
SpriteAnim::GetSprite(const float phase, const Angle& angle) const
{
    GG_CHECKB(std::holds_alternative<LoadedState>(m_State), "Sprite animation is not loaded");

    const LoadedState& state = std::get<LoadedState>(m_State);

    const float fdir = angle.ToTurns() * static_cast<float>(state.m_DirectionCount);
    const uint32_t dir = static_cast<uint32_t>(std::round(fdir)) % state.m_DirectionCount;

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

    const wgpu::Texture& texture = *texResult;
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
    const uint32_t stateCount = spriteRows * spriteCols;
    const uint32_t directionCount = texResult->GetDepthOrArrayLayers();
    const uint32_t spriteCount = directionCount * stateCount;

    BoundedVector<Sprite> sprites(spriteCount);

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

                GG_CHECKB(sprites.push_back(Sprite(texture, rect, layer)), "Failed to add sprite");
            }
        }
    }

    return LoadedState //
        {
            .m_Sprites = std::move(sprites),
            .m_FramesPerDirection = stateCount,
            .m_DirectionCount = directionCount,
        };
}
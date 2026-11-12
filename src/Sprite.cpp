#include "Sprite.h"

/// Sprite

Sprite::Sprite(wgpu::Texture texture, const Rect& rect, const uint32_t layer)
    : m_Texture(std::move(texture)),
      m_Rect(rect),
      m_Layer(layer)
{
}

/// SpriteCollection

SpriteCollection::SpriteCollection(const size_t spriteCount)
    : m_Sprites(spriteCount)
{
}

Result<>
SpriteCollection::AddSprite(wgpu::Texture texture, const Rect& rect, const uint32_t layer)
{
    GG_CHECKB(rect.w > 0 && rect.h > 0, "Invalid sprite dimensions: {}x{}", rect.w, rect.h);

    const uint32_t texW = texture.GetWidth(), texH = texture.GetHeight();

    GG_CHECKB(rect.x + rect.w <= texW && rect.y + rect.h <= texH,
        "Sprite rect out of texture bounds: rect({},{},{},{}) tex({},{})",
        rect.x,
        rect.y,
        rect.w,
        rect.h,
        texW,
        texH);

    GG_CHECKB(m_Sprites.push_back(Sprite(std::move(texture), rect, layer)));

    return Result<>::Success();
}

/// SpriteAnim

Result<SpriteAnim>
SpriteAnim::Create(const SpriteCollection& sprites, const Descriptor& desc)
{
    GG_CHECKB(desc.StatesPerDirection > 0 && desc.DirectionCount > 0,
        "Invalid sprite animation descriptor: state count and direction count must be greater than 0");
    GG_CHECKB(sprites.size() != 0, "Sprite collection must not be empty");
    GG_CHECKB(sprites.size() / desc.StatesPerDirection >= desc.DirectionCount,
        "Sprite collection does not contain enough sprites for the animation descriptor");

    return SpriteAnim(sprites, desc);
}

SpriteAnim::SpriteAnim(const SpriteCollection& sprites, const Descriptor& desc)
    : m_Sprites(&sprites),
      m_StatesPerDirection(desc.StatesPerDirection),
      m_DirectionCount(desc.DirectionCount)
{
}

Result<const Sprite*>
SpriteAnim::GetFrame(const uint32_t state, const uint32_t direction) const
{
    GG_CHECKB(state < m_StatesPerDirection, "Invalid animation state: {}", state);
    GG_CHECKB(direction < m_DirectionCount, "Invalid animation direction: {}", direction);
    
    const size_t index = (direction * m_StatesPerDirection) + state;
    const Sprite* sprite = m_Sprites->TryGetSprite(index);
    GG_CHECKB(sprite, "Invalid sprite index: {}", index);

    return sprite;
}
#include "Prop.h"

#include "Renderer.h"

Prop::Prop(Sprite::Instance spriteInstance, const Transform2D& transform)
    : m_SpriteInstance(spriteInstance),
      m_Transform(transform)
{
}

Result<>
Prop::Render(Renderer& renderer) const
{
    GG_CHECK(renderer.QueueSprite(GetSpriteFrame(), GetTransform()), "Failed to queue sprite");

    return Result<>::Ok;
}
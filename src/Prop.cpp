#include "Prop.h"

#include "Renderer.h"

Prop::Prop(SpriteFrameSequence spriteSeq, const Transform2D& transform)
    : m_SpriteSeq(std::move(spriteSeq)),
      m_Transform(transform)
{
}

Result<>
Prop::Render(Renderer& renderer) const
{
    GG_CHECK(renderer.QueueSprite(GetSpriteFrame(), GetTransform()),
        "Failed to queue sprite");

    return Result<>::Ok;
}
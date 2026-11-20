#include "Prop.h"

#include "Renderer.h"

Prop::Prop(Sprite sprite, const Transform2D& transform)
    : m_Sprite(std::move(sprite)),
      m_Transform(transform)
{
}

Result<>
Prop::Render(Renderer& renderer) const
{
    GG_CHECK(renderer.QueueSprite(m_Sprite, GetTransform()),
        "Failed to queue sprite");

    return Result<>::Ok;
}

Rect
Prop::GetBounds() const
{
    return m_Sprite.GetRect();
}
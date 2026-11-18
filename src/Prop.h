#pragma once

#include "Sprite.h"
#include "Transform2D.h"

class Renderer;

class Prop : public Transform2DLike<Prop>
{
public:
    Prop(Sprite sprite, const Transform2D& transform);

    const Sprite& GetSprite() const { return m_Sprite; }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    Result<> Render(Renderer& renderer) const;

private:
    Sprite m_Sprite;
    Transform2D m_Transform;
};
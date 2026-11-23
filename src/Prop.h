#pragma once

#include "Sprite.h"
#include "Transform2D.h"

class Renderer;

class Prop : public Transform2DLike<Prop>
{
public:
    Prop(Sprite::Instance spriteInstance, const Transform2D& transform);

    SpriteFrame GetSpriteFrame() const { return m_SpriteInstance.GetFrame(); }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    Result<> Render(Renderer& renderer) const;

private:
    Sprite::Instance m_SpriteInstance;
    Transform2D m_Transform;
};
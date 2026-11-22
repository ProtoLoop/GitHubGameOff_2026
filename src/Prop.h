#pragma once

#include "Sprite.h"
#include "Transform2D.h"

class Renderer;

class Prop : public Transform2DLike<Prop>
{
public:
    Prop(SpriteFrameSequence spriteSeq, const Transform2D& transform);

    SpriteFrame GetSpriteFrame() const { return *m_SpriteSeq.GetFrame(0); }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    Result<> Render(Renderer& renderer) const;

private:
    SpriteFrameSequence m_SpriteSeq;
    Transform2D m_Transform;
};
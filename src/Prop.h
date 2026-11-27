#pragma once

#include "EntityId.h"
#include "Sprite.h"
#include "Transform2D.h"

class Renderer;

class Prop : public Transform2DLike<Prop>
{
public:
    Prop() = delete;
    Prop(Sprite::Instance spriteInstance, const Transform2D& transform);
    ~Prop() = default;
    Prop(const Prop&) = delete;
    Prop& operator=(const Prop&) = delete;
    Prop(Prop&&) = default;
    Prop& operator=(Prop&&) = default;

    EntityId GetEntityId() const { return m_Id; }

    SpriteFrame GetSpriteFrame() const { return m_SpriteInstance.GetFrame(); }

    Rectf GetCollisionBounds() const { return GetSpriteFrame().GetCollisionBounds(); }

    std::span<const Rectf> GetCollisionRects() const
    {
        return GetSpriteFrame().GetCollisionRects();
    }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    Result<> Render(Renderer& renderer) const;

private:
    EntityId m_Id{ EntityId::NextId() };
    Sprite::Instance m_SpriteInstance;
    Transform2D m_Transform;
};
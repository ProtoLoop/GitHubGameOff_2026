#include "Nodes.h"

void
SpriteAnimNode::Play(const SpriteAnim::Instance& instance)
{
    m_CurAnim = instance;
}

std::string_view
SpriteAnimNode::WhatsPlaying() const
{
    return m_CurAnim.GetName();
}

SpriteAnimNode&
SpriteAnimNode::FaceDirection(const Angle& facing)
{
    m_CurAnim.SetAngle(facing);
    return *this;
}

Angle
SpriteAnimNode::GetFaceDirection() const
{
    return m_CurAnim.GetAngle();
}

/// Sets the period of the current animation instance.
void
SpriteAnimNode::SetPeriod(const float period)
{
    m_CurAnim.SetPeriod(period);
}

const SpriteAnim::Instance&
SpriteAnimNode::GetCurrentAnim() const
{
    return m_CurAnim;
}

const Sprite&
SpriteAnimNode::GetCurrentSprite() const
{
    const SpriteAnim::Instance& animInstance = GetCurrentAnim();
    return animInstance.GetFrame();
}

void
SpriteAnimNode::Tick(const float deltaTime)
{
    m_CurAnim.Tick(deltaTime);
}

BorrowedHandle<Node2D>
SpriteAnimNode::GetNode()
{
    return m_Node;
}

BorrowedHandle<const Node2D>
SpriteAnimNode::GetNode() const
{
    return m_Node;
}
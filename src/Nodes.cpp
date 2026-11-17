#include "Nodes.h"

// Level.h is needed in order to instantiate OwnedNodeHandle<Node2D> and related types.
// But clang-tidy/IWYU might complain if we don't include it explicitly.
#include "Level.h" // IWYU pragma: keep

SpriteAnimNode::SpriteAnimNode(OwnedNodeHandle<Node2D>&& node, const SpriteAnim::Instance& instance)
    : m_Node(std::move(node)),
      m_CurAnim(instance)
{
}

SpriteAnimNode::~SpriteAnimNode() = default;

SpriteAnimNode::SpriteAnimNode(SpriteAnimNode&&) noexcept = default;
SpriteAnimNode& SpriteAnimNode::operator=(SpriteAnimNode&&) noexcept = default;

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

Sprite
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
    return m_Node.Borrow();
}

BorrowedHandle<const Node2D>
SpriteAnimNode::GetNode() const
{
    return m_Node.Borrow();
}
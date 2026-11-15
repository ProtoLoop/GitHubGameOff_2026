#include "Nodes.h"

Result<SpriteAnimNode>
SpriteAnimNode::Create(Node2D& node, BoundedVector<SpriteAnim::Instance>&& animInstances)
{
    GG_CHECKB(!std::empty(animInstances), "No animation instances provided");

    return SpriteAnimNode(node, std::move(animInstances));
}

Node2D&
SpriteAnimNode::GetNode()
{
    return *m_Node;
}

const Node2D&
SpriteAnimNode::GetNode() const
{
    return *m_Node;
}

Result<>
SpriteAnimNode::Play(const std::string_view& animName)
{
    size_t foundInstance = m_AnimInstances.size();

    size_t curInstance = 0;

    for(const SpriteAnim::Instance& instance : m_AnimInstances)
    {
        if(instance.GetName() == animName)
        {
            foundInstance = curInstance;
            break;
        }
        ++curInstance;
    }

    GG_CHECKB(foundInstance != m_AnimInstances.size(), "Animation not found: {}", animName);

    if(foundInstance != m_CurAnimIndex)
    {
        // Reset the phase of the new animation.
        m_AnimInstances.TryGet(foundInstance)->SetPhase(0);
    }

    m_CurAnimIndex = foundInstance;
    return Result<>::Ok;
}

std::string_view
SpriteAnimNode::WhatsPlaying() const
{
    return m_AnimInstances.TryGet(m_CurAnimIndex)->GetName();
}

SpriteAnimNode&
SpriteAnimNode::FaceDirection(const Angle& facing)
{
    m_AnimInstances.TryGet(m_CurAnimIndex)->SetAngle(facing);
    return *this;
}

Angle
SpriteAnimNode::GetFaceDirection() const
{
    return m_AnimInstances.TryGet(m_CurAnimIndex)->GetAngle();
}

/// Sets the period of the current animation instance.
void
SpriteAnimNode::SetPeriod(const float period)
{
    m_AnimInstances.TryGet(m_CurAnimIndex)->SetPeriod(period);
}

const SpriteAnim::Instance&
SpriteAnimNode::GetCurrentAnim() const
{
    return *m_AnimInstances.TryGet(m_CurAnimIndex);
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
    m_AnimInstances.TryGet(m_CurAnimIndex)->Tick(deltaTime);
}
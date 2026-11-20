#include "Sprite.h"

#include "Angle.h"

/// Sprite

Sprite::Sprite(wgpu::BindGroup bindGroup,
    const Rect& rect,
    const glm::vec2& pivot,
    const glm::vec4& color,
    const float scale)
    : m_TextureBindGroup(std::move(bindGroup)),
      m_Rect(rect),
      m_Pivot(pivot),
      m_Color(color),
      m_Scale(scale)
{
}

/// SpriteSequence

SpriteSequence::SpriteSequence(const ResourceName& name,
    wgpu::BindGroup bindGroup,
    const Dimension2D& dim,
    const Rect& rect,
    const glm::vec2& pivot,
    const glm::vec4& color)
    : m_Name(name),
      m_BindGroup(std::move(bindGroup)),
      m_Dim(dim),
      m_Rect(rect),
      m_Pivot(pivot),
      m_Color(color),
      m_SpriteCount((rect.w / dim.w) * (rect.h / dim.h))
{
}

Result<Sprite>
SpriteSequence::GetSprite(const uint32_t index) const
{
    GG_CHECKB(index < m_SpriteCount, "Index out of bounds");

    const uint32_t spritesPerRow = m_Rect.w / m_Dim.w;
    const uint32_t row = index / spritesPerRow;
    const uint32_t col = index % spritesPerRow;
    const Rect rect //
        {
            m_Rect.x + (col * m_Dim.w),
            m_Rect.y + (row * m_Dim.h),
            m_Dim.w,
            m_Dim.h,
        };

    return Sprite(m_BindGroup, rect, m_Pivot, m_Color, 1.0f);
}

/// SpriteAnim

SpriteAnim::SpriteAnim(const ResourceName& name, BoundedVector<SpriteSequence>&& spriteSeqs)
    : m_Name(name),
      m_SpriteSequences(std::move(spriteSeqs))
{
}

const ResourceName&
SpriteAnim::GetName() const
{
    return m_Name;
}

Result<SpriteAnim::Instance>
SpriteAnim::CreateInstance() const
{
    return Instance(*this);
}

Sprite
SpriteAnim::GetSprite(const float phase, const Angle& angle) const
{
    const size_t dirCount = m_SpriteSequences.size();
    const float fdir = angle.ToTurns() * static_cast<float>(dirCount);
    const size_t direction = static_cast<size_t>(std::round(fdir)) % dirCount;

    const SpriteSequence& sequence = m_SpriteSequences[direction];

    const uint32_t framesPerDirection = sequence.GetSpriteCount();
    const float normalizedPhase = phase - std::floor(phase);
    const uint32_t frame =
        static_cast<uint32_t>(normalizedPhase * static_cast<float>(framesPerDirection))
        % framesPerDirection;

    return *sequence.GetSprite(frame);
}

/// SpriteAnim::Instance

SpriteAnim::Instance::Instance(const SpriteAnim& anim)
    : m_Anim(&anim)
{
}

const ResourceName&
SpriteAnim::Instance::GetName() const
{
    return m_Anim->GetName();
}

void
SpriteAnim::Instance::Tick(const float deltaTime)
{
    SetPhase(m_Phase + (deltaTime / m_Period));
}

void
SpriteAnim::Instance::SetPeriod(const float period)
{
    constexpr float minPeriod = 1e-6f; // Minimum allowed animation period
    if(GG_VERIFYB(period > minPeriod, "Animation period must be positive"))
    {
        m_Period = period;
    }
}

float
SpriteAnim::Instance::GetPeriod() const
{
    return m_Period;
}

void
SpriteAnim::Instance::SetPhase(const float phase)
{
    m_Phase = phase - std::floor(phase);
}

float
SpriteAnim::Instance::GetPhase() const
{
    return m_Phase;
}

void
SpriteAnim::Instance::SetAngle(const Angle& angle)
{
    m_Angle = angle;
}

const Angle&
SpriteAnim::Instance::GetAngle() const
{
    return m_Angle;
}

Sprite
SpriteAnim::Instance::GetSprite() const
{
    return m_Anim->GetSprite(m_Phase, m_Angle);
}

Sprite
SpriteAnim::Instance::GetSprite(const float phase, const Angle& angle) const
{
    return m_Anim->GetSprite(phase, angle);
}
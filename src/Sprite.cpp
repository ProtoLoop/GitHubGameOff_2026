#include "Sprite.h"

#include "Angle.h"

namespace
{
uint32_t
ComputeFrameCount(const Rect& srcRect, const Dimension2D& size)
{
    if(GG_VERIFYB(size.w > 0 && size.h > 0, "Invalid sprite size: {}x{}", size.w, size.h))
    {
        return (srcRect.w / size.w) * (srcRect.h / size.h);
    }
    return 0;
}

/// Computes the bounding rectangle that encompasses all the given collision rectangles.
/// If colRects is empty, returns a rectangle with zero size at the origin.
Rectf
ComputeCollisionBounds(const std::span<const Rectf> colRects, const glm::vec2 spriteSize)
{
    if(colRects.empty())
    {
        return Rectf(0, 0, 0, 0);
    }

    Rectf bounds = colRects[0];
    for(size_t i = 1; i < colRects.size(); ++i)
    {
        const Rectf& r = colRects[i];
        const float x1 = std::min(bounds.x, r.x);
        const float y1 = std::min(bounds.y, r.y);
        const float x2 = std::max(bounds.x + bounds.w, r.x + r.w);
        const float y2 = std::max(bounds.y + bounds.h, r.y + r.h);
        bounds.x = x1;
        bounds.y = y1;
        bounds.w = x2 - x1;
        bounds.h = y2 - y1;
    }

    return Rectf(bounds.x * spriteSize.x,
        bounds.y * spriteSize.y,
        bounds.w * spriteSize.x,
        bounds.h * spriteSize.y);
}
} // namespace

/// Sprite

SpriteFrame::SpriteFrame(wgpu::BindGroup bindGroup,
    const Rect& srcRect,
    const std::span<const Rectf> colRects,
    const Rectf& collisionBounds,
    const glm::vec2& pivot,
    const glm::vec4& tint,
    const float scale)
    : m_TextureBindGroup(std::move(bindGroup)),
      m_SrcRect(srcRect),
      m_ColRects(colRects),
      m_CollisionBounds(collisionBounds),
      m_Pivot(pivot),
      m_Tint(tint),
      m_Scale(scale)
{
}

Rectf
SpriteFrame::GetScreenRect(
    const Rectf& rect,
    const glm::vec2& cellXY,
    const glm::mat2x2 proj,
    const float cellPixPerSide) const
{
    // Y flip when going from map space +Y up/north, to screen space +Y down.
    const glm::vec2 pixelScale = glm::vec2(1, -1) * cellPixPerSide;

    const Rectf rs = rect * GetScale();
    const glm::vec2 pivot = GetPivot() * glm::vec2(GetSize()) * GetScale();
    const glm::vec2 origin = rs.Origin() + ((proj * cellXY * pixelScale) - pivot);

    return Rectf(origin, rs.Size());
}

Rectf
SpriteFrame::GetCollisionScreenRect(const uint32_t rectIndex,
    const glm::vec2& cellXY,
    const glm::mat2x2 proj,
    const float cellPixPerSide) const
{
    if(!GG_VERIFYB(rectIndex < m_ColRects.size(),
           "Invalid collision rectangle index: {}",
           rectIndex))
    {
        return Rectf(0, 0, 0, 0);
    }

    const Rectf& colRect = m_ColRects[rectIndex];
    const glm::vec2 size(GetSize());
    const Rectf r = Rectf(colRect.Origin() * size, colRect.Size() * size);
    return GetScreenRect(r, cellXY, proj, cellPixPerSide);
}

/// SpriteSequence

SpriteFrameSequence::SpriteFrameSequence(const ResourceName& name,
    wgpu::BindGroup bindGroup,
    const Dimension2D& size,
    const Rect& srcRect,
    const InlineVector<Rectf, 1>& colRects,
    const glm::vec2& pivot,
    const glm::vec4& tint,
    const float scale)
    : m_Name(name),
      m_BindGroup(std::move(bindGroup)),
      m_Size(size),
      m_SrcRect(srcRect),
      m_ColRects(colRects),
      m_CollisionBounds(ComputeCollisionBounds(colRects, glm::vec2(size.w, size.h))),
      m_Pivot(pivot),
      m_Tint(tint),
      m_Scale(scale),
      m_FrameCount(ComputeFrameCount(srcRect, size))
{
}

Result<SpriteFrame>
SpriteFrameSequence::GetFrame(const uint32_t index) const
{
    GG_CHECKB(index < m_FrameCount, "Index out of bounds");

    const uint32_t spritesPerRow = m_SrcRect.w / m_Size.w;
    const uint32_t row = index / spritesPerRow;
    const uint32_t col = index % spritesPerRow;
    const Rect rect //
        {
            m_SrcRect.x + (col * m_Size.w),
            m_SrcRect.y + (row * m_Size.h),
            m_Size.w,
            m_Size.h,
        };

    return SpriteFrame(m_BindGroup, rect, m_ColRects, m_CollisionBounds, m_Pivot, m_Tint, m_Scale);
}

/// SpriteAnim

SpriteAnim::SpriteAnim(const ResourceName& name, BoundedVector<SpriteFrameSequence>&& spriteSeqs)
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

SpriteFrame
SpriteAnim::GetFrame(const float phase, const Angle& angle) const
{
    const size_t dirCount = m_SpriteSequences.size();
    const float fdir = angle.ToTurns() * static_cast<float>(dirCount);
    const size_t direction = static_cast<size_t>(std::round(fdir)) % dirCount;

    const SpriteFrameSequence& sequence = m_SpriteSequences[direction];

    const uint32_t framesPerDirection = sequence.GetFrameCount();
    const float normalizedPhase = phase - std::floor(phase);
    const uint32_t frame = GG_VERIFYB(framesPerDirection > 0, "No frames available")
        ? static_cast<uint32_t>(normalizedPhase * static_cast<float>(framesPerDirection))
            % framesPerDirection
        : 0;

    return *sequence.GetFrame(frame);
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

SpriteFrame
SpriteAnim::Instance::GetFrame() const
{
    return m_Anim->GetFrame(m_Phase, m_Angle);
}

SpriteFrame
SpriteAnim::Instance::GetFrame(const float phase, const Angle& angle) const
{
    return m_Anim->GetFrame(phase, angle);
}
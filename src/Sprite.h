#pragma once

#include "Angle.h"
#include "BoundedVector.h"
#include "FileFetcher.h"
#include "Geometry2D.h"
#include "InlineString.h"

#include <glm/mat2x2.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <webgpu/webgpu_cpp.h>

namespace detail
{
constexpr size_t kResourceNameSize = 64;
}

using ResourceName = InlineString<detail::kResourceNameSize>;

/// Defines frames within a rectangular region of a texture.
/// Size specifies each frame's pixel dimensions. Frames advance left to right,
/// then top to bottom. Incomplete columns or rows at the edges are ignored.
struct SpriteFrameSequenceDef
{
    ResourceName Name;
    FilePath TexturePath;
    // If not present then use the entire texture.
    std::optional<Rect> SrcRect = std::nullopt;
    std::optional<Dimension2D> Size = std::nullopt;
    /// Collision rectangles are normalized within the sprite's frame.
    /// 0,0 represents the top-left corner of the frame.
    /// 1,1 represents the bottom-right corner of the frame.
    /// If not present then there is no collision.
    /// Collision rectangles CAN be outside the [0,1] range.
    /// When loaded normalized collision rectangles are converted to the sprite's coordinate space.
    std::optional<Rectf> ColRect = std::nullopt;
    /// Normalized pivot point within the sprite's frame.
    /// 0,0 represents the top-left corner of the frame.
    /// 1,1 represents the bottom-right corner of the frame.
    glm::vec2 Pivot{ 0, 0 };
    glm::vec4 Tint = glm::vec4(1.0f);
    float Scale = 1.0f;
};

/// Defines a sprite.
/// A sprite is a collection of sprite frame sequences, each representing a different facing direction.
/// The sequence at index 0 corresponds to the sprite facing zero turns (North),
/// and subsequent indices advance by 1 / (sequence count) turns.
/// Playback selects the nearest direction based on the angle. Each sequence's frame count
/// spans one complete animation cycle.
struct SpriteDef
{
    ResourceName Name;
    // FIXME(KB) - use a borrwed span from TBD SpriteCatalog
    std::vector<ResourceName> SequenceNames;
};

/// A lightweight representation of a sprite frame within a texture atlas.
/// Intended to be constructed on the fly and returned by value.
class SpriteFrame
{
public:
    SpriteFrame() = delete;

    /// Returns the bind group for the sprite's texture.
    const wgpu::BindGroup& GetTextureBindGroup() const { return m_TextureBindGroup; }

    /// Returns the size (width and height) of the sprite.
    glm::uvec2 GetSize() const { return m_SrcRect.Size(); }

    /// Returns the rectangle defining the sprite within the texture atlas.
    const Rect& GetSrcRect() const { return m_SrcRect; }

    /// Returns the collision rectangles of the sprite.
    /// Collision rectangles are in the same coordinate space as the sprite's rectangle.
    std::span<const Rectf> GetCollisionRects() const { return m_ColRects; }

    /// Returns the bounding rectangle that encompasses all the collision rectangles.
    const Rectf& GetCollisionBounds() const { return m_CollisionBounds; }

    /// Returns the pivot point within the sprite's rectangle.
    const glm::vec2& GetPivot() const { return m_Pivot; }

    /// Returns the scale factor for the sprite.
    float GetScale() const { return m_Scale; }

    /// Returns the color tint for the sprite.
    const glm::vec4& GetTint() const { return m_Tint; }

    Rectf GetScreenRect(const Rectf& rect,
        const glm::vec2& cellXY,
        const glm::mat2x2 proj,
        const float cellPixPerSide) const;

private:
    friend class SpriteFrameSequence;

    /// Constructs a sprite with the given bind group and rectangle.
    /// bindGroup is the bind group for the sprite's texture.
    /// rect specifies the portion of the texture to use for the sprite.
    /// colRects specifies the collision rectangles of the sprite.
    /// pivot specifies the pivot point within the sprite's rectangle.
    /// tint specifies the color tint for the sprite.
    /// scale specifies the scale factor for the sprite.
    SpriteFrame(wgpu::BindGroup bindGroup,
        const Rect& srcRect,
        const std::span<const Rectf> colRects,
        const Rectf& collisionBounds,
        const glm::vec2& pivot,
        const glm::vec4& tint,
        const float scale);

    wgpu::BindGroup m_TextureBindGroup; // Bind group for the sprite's texture.
    Rect m_SrcRect;                     // Source rectangle within the texture atlas.
    // Collection of collision rectangles of the sprite.
    // Collision rectangles are in the same coordinate space as the sprite's rectangle.
    std::span<const Rectf> m_ColRects;
    // Union of collision rectangles - used for quick collision checks.
    Rectf m_CollisionBounds;
    glm::vec2 m_Pivot;
    glm::vec4 m_Tint;
    float m_Scale;
};

class SpriteFrameSequence
{
public:
    SpriteFrameSequence() = delete;

    const ResourceName& GetName() const { return m_Name; }

    /// Returns the rectangle defining the sprite sequence within the texture atlas.
    const Rect& GetSrcRect() const { return m_SrcRect; }

    /// Returns the color tint for the sprite sequence.
    const glm::vec4& GetTint() const { return m_Tint; }

    float GetScale() const { return m_Scale; }

    /// Returns the sprite frame at the specified index within the sequence.
    Result<SpriteFrame> GetFrame(const uint32_t index) const;

    /// Returns the number of frames in the sequence.
    uint32_t GetFrameCount() const { return m_FrameCount; }

private:
    friend class SpriteSequenceLoader;

    SpriteFrameSequence(const ResourceName& name,
        wgpu::BindGroup bindGroup,
        const Dimension2D& size,
        const Rect& srcRect,
        const InlineVector<Rectf, 1>& colRects,
        const glm::vec2& pivot,
        const glm::vec4& tint,
        const float scale);

    ResourceName m_Name;
    wgpu::BindGroup m_BindGroup;
    // W/H of each sprite within the sequence.
    Dimension2D m_Size;
    // Source rectangle of the sequence within the texture atlas.
    Rect m_SrcRect;
    // Collection of collision rectangles of the sprite.
    // Collision rectangles are in the same coordinate space as the sprite's rectangle.
    InlineVector<Rectf, 1> m_ColRects;
    // Union of collision rectangles - used for quick collision checks.
    Rectf m_CollisionBounds;
    glm::vec2 m_Pivot;
    glm::vec4 m_Tint;
    float m_Scale;
    uint32_t m_FrameCount;
};

/// A sprite is a collection of sprite frame sequences, each representing a different facing direction.
/// The sequence at index 0 corresponds to the sprite facing zero turns (North),
/// and subsequent indices advance by 1 / (sequence count) turns.
/// Playback selects the nearest direction based on the angle. Each sequence's frame count
/// spans one complete animation cycle.
class Sprite
{
public:
    class Instance;

    Sprite() = delete;
    ~Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&&) = default;
    Sprite& operator=(Sprite&&) = default;

    /// Returns the name of the sprite.
    const ResourceName& GetName() const;

    /// Creates an instance of the sprite.
    /// The instance can be used to control and query the state.
    Result<Instance> CreateInstance() const;

    /// Returns the sprite frame corresponding to the given phase and angle.
    /// phase is a value between 0 and 1 representing the progress through a sequence.
    /// angle is the direction (which maps to a sequence) for which to retrieve the sprite.
    SpriteFrame GetFrame(const float phase, const Angle& angle) const;

private:
    friend class LevelLoader;

    Sprite(const ResourceName& name, BoundedVector<const SpriteFrameSequence*>&& spriteSeqs);

    ResourceName m_Name;
    // FIXME(KB) - borrow storage from global array of sequences.
    BoundedVector<const SpriteFrameSequence*> m_SpriteSequences;
};

/// Represents an instance of a sprite.
/// An instance of a sprite can be used to control and query the sprite state, such as
/// its current phase and angle.
/// Borrows a Sprite, which must remain alive at the same address for the
/// lifetime of this instance.
class Sprite::Instance
{
public:
    Instance() = delete;

    const ResourceName& GetName() const;

    /// Advances the animation by delta time.
    /// deltaTime is the time elapsed since the last update.
    void Tick(const float deltaTime);

    /// Sets the period/duration of one animation cycle.
    void SetPeriod(const float period);

    /// Returns the period/duration of one animation cycle.
    float GetPeriod() const;

    /// Sets the current animation phase within a cycle.
    /// phase is a value between 0 and 1.
    void SetPhase(const float phase);

    /// Returns the current animation phase within a cycle.
    /// The returned value is between 0 and 1.
    float GetPhase() const;

    /// Sets the current animation angle - the direction the sprite should face.
    void SetAngle(const Angle& angle);

    /// Returns the current animation angle - the direction the sprite is facing.
    const Angle& GetAngle() const;

    /// Returns the sprite frame corresponding to the current animation phase and angle.
    SpriteFrame GetFrame() const;

    /// Returns the sprite frame corresponding to the given animation phase and angle.
    /// phase is a value between 0 and 1 representing the animation progress.
    /// angle is the direction for which to retrieve the sprite.
    SpriteFrame GetFrame(const float phase, const Angle& angle) const;

private:
    friend Sprite;

    explicit Instance(const Sprite& anim);

    const Sprite* m_Sprite;

    float m_Period{ 1.0f }; // Duration of one animation cycle.
    float m_Phase{ 0.0f };
    Angle m_Angle;
};

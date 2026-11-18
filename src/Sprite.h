#pragma once

#include "Angle.h"
#include "BoundedVector.h"
#include "InlineString.h"
#include "TextureLoader.h"

#include <span>
#include <webgpu/webgpu_cpp.h>

class WebGpu;
class Renderer;

namespace detail
{
constexpr size_t kResourceNameSize = 64;
}

using ResourceName = InlineString<detail::kResourceNameSize>;

struct Rect
{
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;

    friend bool operator!=(const Rect& lhs, const Rect& rhs) = default;
    friend bool operator==(const Rect& lhs, const Rect& rhs) = default;
};

struct Dimension2D
{
    uint32_t w;
    uint32_t h;

    friend bool operator!=(const Dimension2D& lhs, const Dimension2D& rhs) = default;
    friend bool operator==(const Dimension2D& lhs, const Dimension2D& rhs) = default;
};

struct SpriteDef
{
    ResourceName Name;
    FilePath TexturePath;
    // If not present then use the entire texture.
    std::optional<Rect> Rect = std::nullopt;
};

/// Defines frames within a rectangular region of a texture.
/// Dim specifies each frame's pixel dimensions. Frames advance left to right,
/// then top to bottom. Incomplete columns or rows at the edges are ignored.
struct SpriteSequenceDef
{
    ResourceName Name;
    Dimension2D Dim{};
    FilePath TexturePath;
    // If not present then use the entire texture.
    std::optional<Rect> Rect = std::nullopt;
};

/// Defines an animation's equally spaced facing directions.
/// SequenceDefs order determines direction: index 0 corresponds to zero turns,
/// and subsequent indices advance by 1 / SequenceDefs.size() turns.
/// Playback selects the nearest direction. Each sequence's own frame count
/// spans one complete animation cycle.
struct SpriteAnimDef
{
    ResourceName Name;
    /// Borrows sequence storage, which must remain valid whenever this definition
    /// is read. SpriteAnimLoader::Load copies the data it needs before returning.
    std::span<const SpriteSequenceDef> SequenceDefs;
};

class NamedSprite;

class SpriteLoader
{
public:
    SpriteLoader() = delete;
    ~SpriteLoader() = default;
    SpriteLoader(const SpriteLoader&) = delete;
    SpriteLoader& operator=(const SpriteLoader&) = delete;
    SpriteLoader(SpriteLoader&&) = default;
    SpriteLoader& operator=(SpriteLoader&&) = default;

    static Result<SpriteLoader> Load(const std::span<const SpriteDef> spriteDefs,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<BoundedVector<NamedSprite>> Take();

private:
    SpriteLoader(BoundedVector<TextureLoader>&& textureLoaders,
        BoundedVector<size_t>&& spriteToTextureMap,
        BoundedVector<NamedSprite>&& sprites,
        Renderer& renderer);

    BoundedVector<TextureLoader> m_TextureLoaders;
    BoundedVector<size_t> m_SpriteToTextureMap;
    BoundedVector<NamedSprite> m_Sprites;
    Renderer* m_Renderer;

    bool m_Taken{ false };
};

/// A lightweight representation of a sprite within a texture atlas.
/// Intended to be constructed on the fly and returned by value.
class Sprite
{
public:
    Sprite() = delete;

    /// Returns the bind group for the sprite's texture.
    const wgpu::BindGroup& GetTextureBindGroup() const { return m_TextureBindGroup; }

    /// Returns the rectangle defining the sprite within the texture atlas.
    const Rect& GetRect() const { return m_Rect; }

private:
    friend class SpriteAnimLoader;
    friend class SpriteSequence;
    friend class SpriteLoader;

    /// Constructs a sprite with the given bind group and rectangle.
    /// bindGroup is the bind group for the sprite's texture.
    /// rect specifies the portion of the texture to use for the sprite.
    Sprite(wgpu::BindGroup bindGroup, const Rect& rect);

    wgpu::BindGroup m_TextureBindGroup; // Bind group for the sprite's texture.
    Rect m_Rect;
};

class NamedSprite
{
public:
    NamedSprite() = delete;

    NamedSprite(const ResourceName& name, Sprite&& sprite)
        : m_Name(name),
          m_Sprite(std::move(sprite))
    {
    }

    const ResourceName& GetName() const { return m_Name; }

    /// Returns the bind group for the sprite's texture.
    const wgpu::BindGroup& GetTextureBindGroup() const { return m_Sprite.GetTextureBindGroup(); }

    /// Returns the rectangle defining the sprite within the texture atlas.
    const Rect& GetRect() const { return m_Sprite.GetRect(); }

    const Sprite& GetSprite() const { return m_Sprite; }

private:
    ResourceName m_Name;
    Sprite m_Sprite;
};

class SpriteSequence
{
public:
    SpriteSequence() = delete;

    const ResourceName& GetName() const { return m_Name; }

    const Dimension2D& GetDim() const { return m_Dim; }

    const Rect& GetRect() const { return m_Rect; }

    Result<Sprite> GetSprite(const uint32_t index) const;

    uint32_t GetSpriteCount() const { return m_SpriteCount; }

private:
    friend class SpriteAnimLoader;

    SpriteSequence(const ResourceName& name,
        wgpu::BindGroup bindGroup,
        const Dimension2D& dim,
        const Rect& rect);

    ResourceName m_Name;
    wgpu::BindGroup m_BindGroup;
    Dimension2D m_Dim;
    Rect m_Rect;
    uint32_t m_SpriteCount;
};

class SpriteAnim;

class SpriteAnimLoader
{
public:
    SpriteAnimLoader() = delete;
    ~SpriteAnimLoader() = default;
    SpriteAnimLoader(const SpriteAnimLoader&) = delete;
    SpriteAnimLoader& operator=(const SpriteAnimLoader&) = delete;
    SpriteAnimLoader(SpriteAnimLoader&&) = default;
    SpriteAnimLoader& operator=(SpriteAnimLoader&&) = default;

    static Result<SpriteAnimLoader> Load(
        const SpriteAnimDef& animDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu);

    const ResourceName& GetName() const;

    bool IsPending() const;

    Result<> Update();

    Result<SpriteAnim> Take();

private:
    SpriteAnimLoader(const ResourceName& name,
        BoundedVector<TextureLoader>&& textureLoaders,
        BoundedVector<size_t>&& sequenceToTextureMap,
        BoundedVector<SpriteSequence>&& spriteSequences,
        Renderer& renderer);

    ResourceName m_Name;
    BoundedVector<TextureLoader> m_TextureLoaders;
    BoundedVector<size_t> m_SequenceToTextureMap;
    BoundedVector<SpriteSequence> m_SpriteSequences;
    Renderer* m_Renderer;

    bool m_Taken{ false };
};

/// A sprite animation resource.
/// It defines the frames, directions, and textures for a sprite animation.
/// To control individual instances of the animation create an Instance using CreateInstance().
class SpriteAnim
{
public:
    class Instance;

    SpriteAnim() = delete;
    ~SpriteAnim() = default;
    SpriteAnim(const SpriteAnim&) = delete;
    SpriteAnim& operator=(const SpriteAnim&) = delete;
    SpriteAnim(SpriteAnim&&) = default;
    SpriteAnim& operator=(SpriteAnim&&) = default;

    /// Returns the name of the sprite animation.
    const ResourceName& GetName() const;

    /// Creates an instance of the sprite animation.
    /// The instance can be used to control and query the animation state.
    Result<Instance> CreateInstance() const;

    /// Returns the sprite frame corresponding to the given animation phase and angle.
    /// phase is a value between 0 and 1 representing the animation progress.
    /// angle is the direction for which to retrieve the sprite.
    Sprite GetFrame(const float phase, const Angle& angle) const;

private:
    friend class SpriteAnimLoader;

    SpriteAnim(const ResourceName& name, BoundedVector<SpriteSequence>&& spriteSeqs);

    ResourceName m_Name;
    BoundedVector<SpriteSequence> m_SpriteSequences;
};

/// Represents an instance of a sprite animation.
/// An instance of a sprite animation can be used to control and query the animation state, such as
/// its current phase and angle.
/// Borrows a SpriteAnim, which must remain alive at the same address for the
/// lifetime of this instance.
class SpriteAnim::Instance
{
public:
    Instance() = delete;

    const ResourceName& GetName() const;

    /// Advances the animation by delta time.
    /// deltaTime is the time elapsed since the last update.
    void Tick(const float deltaTime);

    /// Sets the period/duration of one animation cycle.
    void SetPeriod(const float period);

    /// Sets the current animation phase within a cycle.
    /// phase is a value between 0 and 1.
    void SetPhase(const float phase);

    /// Sets the current animation angle - the direction the sprite should face.
    void SetAngle(const Angle& angle);

    /// Returns the current animation angle - the direction the sprite is facing.
    const Angle& GetAngle() const;

    /// Returns the sprite frame corresponding to the current animation phase and angle.
    Sprite GetFrame() const;

private:
    friend SpriteAnim;

    explicit Instance(const SpriteAnim& anim);

    const SpriteAnim* m_Anim;

    float m_Period{ 1.0f }; // Duration of one animation cycle.
    float m_Phase{ 0.0f };
    Angle m_Angle;
};

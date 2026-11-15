#pragma once

#include "Angle.h"
#include "BoundedVector.h"
#include "InlineString.h"
#include "TextureLoader.h"

#include <vector>
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
};

struct Dimension2d
{
    uint32_t w;
    uint32_t h;
};

struct SpriteAnimDef
{
    ResourceName Name;
    Dimension2d Dim{};
    std::vector<FilePath> Textures;
    uint32_t DirectionCount() const { return static_cast<uint32_t>(Textures.size()); }
};

class Sprite
{
public:
    Sprite() = delete;

    /// Returns the bind group for the sprite's texture.
    const wgpu::BindGroup& GetTextureBindGroup() const { return m_TextureBindGroup; }

    /// Returns the rectangle defining the sprite within the texture atlas.
    const Rect& GetRect() const { return m_Rect; }

    /// Returns the layer of the sprite within the texture atlas.
    /// Often different directions of the same sprite are stored in different layers.
    uint32_t GetLayer() const { return m_Layer; }

private:
    friend class SpriteAnimLoader;

    /// Constructs a sprite with the given bind group, rectangle, and layer.
    /// bindGroup is the bind group for the sprite's texture.
    /// rect specifies the portion of the texture to use for the sprite.
    Sprite(wgpu::BindGroup bindGroup, const Rect& rect, const uint32_t layer);

    wgpu::BindGroup m_TextureBindGroup; // Bind group for the sprite's texture.
    Rect m_Rect;
    uint32_t m_Layer; // Layer in the texture array.
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
    SpriteAnimLoader(const ResourceName& Name,
        TextureLoader&& m_TextureLoader,
        const Dimension2d& m_Dim,
        Renderer* m_Renderer);

    ResourceName m_Name;
    TextureLoader m_TextureLoader;
    Dimension2d m_Dim;
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
    const Sprite& GetFrame(const float phase, const Angle& angle) const;

private:
    friend class SpriteAnimLoader;

    ResourceName Name;
    wgpu::Texture m_Texture;
    BoundedVector<Sprite> m_Sprites;
    uint32_t m_FramesPerDirection{ 0 };

    uint32_t GetDirectionCount() const
    {
        return m_Texture.GetDepthOrArrayLayers();
    }

    SpriteAnim(const ResourceName& Name,
        wgpu::Texture&& m_Texture,
        BoundedVector<Sprite>&& m_Sprites,
        const uint32_t m_FramesPerDirection);
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
    const Sprite& GetFrame() const;

private:
    friend SpriteAnim;

    explicit Instance(const SpriteAnim& anim);

    const SpriteAnim* m_Anim;

    float m_Period{ 1.0f }; // Duration of one animation cycle.
    float m_Phase{ 0.0f };
    Angle m_Angle;
};
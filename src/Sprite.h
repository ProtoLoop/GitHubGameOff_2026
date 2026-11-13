#pragma once

#include "BoundedVector.h"
#include "TextureLoader.h"

#include <variant>
#include <vector>
#include <webgpu/webgpu_cpp.h>

class WebGpu;
class Renderer;
class Angle;

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
    Dimension2d Dim{};
    std::vector<FilePath> Textures;
    uint32_t DirectionCount() const { return static_cast<uint32_t>(Textures.size()); }
};

class Sprite
{
public:
    Sprite() = delete;

    /// Constructs a sprite with the given bind group, rectangle, and layer.
    /// bindGroup is the bind group for the sprite's texture.
    /// rect specifies the portion of the texture to use for the sprite.
    Sprite(wgpu::BindGroup bindGroup, const Rect& rect, const uint32_t layer);

    /// Returns the bind group for the sprite's texture.
    const wgpu::BindGroup& GetTextureBindGroup() const { return m_TextureBindGroup; }

    /// Returns the rectangle defining the sprite within the texture atlas.
    const Rect& GetRect() const { return m_Rect; }

    /// Returns the layer of the sprite within the texture atlas.
    /// Often different directions of the same sprite are stored in different layers.
    uint32_t GetLayer() const { return m_Layer; }

private:
    wgpu::BindGroup m_TextureBindGroup; // Bind group for the sprite's texture.
    Rect m_Rect;
    uint32_t m_Layer; // Layer in the texture array.
};

class SpriteAnim
{
public:
    SpriteAnim() = delete;

    static Result<SpriteAnim> Load(
        const SpriteAnimDef& animDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    /// Snaps the given angle to the nearest direction supported by the animation.
    Result<Angle> SnapAngle(const Angle& angle) const;

    /// Returns the sprite corresponding to the given animation phase and angle.
    /// phase is a value between 0 and 1 representing the animation progress.
    /// angle is the direction for which to retrieve the sprite.
    Result<const Sprite*> GetSprite(const float phase, const Angle& angle) const;

private:
    struct LoadingState
    {
        TextureLoader m_TextureLoader;
        Dimension2d m_Dim;
        Renderer* m_Renderer;
    };

    struct LoadedState
    {
        BoundedVector<Sprite> m_Sprites;
        uint32_t m_FramesPerDirection{ 0 };
        uint32_t m_DirectionCount{ 0 };
    };

    explicit SpriteAnim(Renderer& renderer, TextureLoader textureLoader, const Dimension2d& dim);

    Result<LoadedState> BuildSprites();

    std::variant<std::monostate, LoadingState, LoadedState> m_State;
};
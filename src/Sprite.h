#pragma once

#include "BoundedVector.h"
#include "TextureLoader.h"

#include <variant>
#include <webgpu/webgpu_cpp.h>
#include <vector>

class WebGpu;

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

    /// Constructs a sprite with the given texture, rectangle, and layer.
    /// The texture could be multi-layered and layer specifies which layer of the texture to use.
    /// rect specifies the portion of the texture to use for the sprite.
    Sprite(wgpu::Texture texture, const Rect& rect, const uint32_t layer);

    /// Returns the texture atlas containing the sprite.
    const wgpu::Texture& GetTexture() const { return m_Texture; }

    /// Returns the rectangle defining the sprite within the texture atlas.
    const Rect& GetRect() const { return m_Rect; }

    /// Returns the layer of the sprite within the texture atlas.
    /// Often different directions of the same sprite are stored in different layers.
    uint32_t GetLayer() const { return m_Layer; }

private:
    wgpu::Texture m_Texture; // Texture containing the sprite, possibly multilayer.
    Rect m_Rect;
    uint32_t m_Layer; // Layer in the texture array.
};

class SpriteAnim
{
public:
    SpriteAnim() = delete;

    static Result<SpriteAnim> Create(const SpriteAnimDef& animDef, FileFetcher& fileFetcher, WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    /// Returns the number of animation frames per direction.
    uint32_t GetFramesPerDirection() const
    {
        return std::get<LoadedState>(m_State).m_FramesPerDirection;
    }

    /// Returns the number of directions in the animation.
    uint32_t GetDirectionCount() const { return std::get<LoadedState>(m_State).m_DirectionCount; }

    /// Returns the sprite for the given animation state and direction.
    /// The frame should be in the range [0, FramesPerDirection) and the direction should be in the
    /// range [0, DirectionCount).
    Result<const Sprite*> GetSprite(const uint32_t frame, const uint32_t direction) const;

private:

    struct LoadingState
    {
        TextureLoader m_TextureLoader;
        Dimension2d m_Dim;
    };

    struct LoadedState
    {
        BoundedVector<Sprite> m_Sprites;
        uint32_t m_FramesPerDirection{ 0 };
        uint32_t m_DirectionCount{ 0 };
    };

    explicit SpriteAnim(TextureLoader textureLoader, const Dimension2d& dim);

    Result<LoadedState> BuildSprites();

    std::variant<std::monostate,LoadingState, LoadedState> m_State;

};
#pragma once

#include "BoundedVector.h"

#include <webgpu/webgpu_cpp.h>

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

class Sprite
{
public:
    Sprite() = delete;

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

class SpriteCollection
{
public:
    SpriteCollection() = delete;

    explicit SpriteCollection(const size_t spriteCount);

    Result<> AddSprite(wgpu::Texture texture, const Rect& rect, const uint32_t layer);

    size_t size() const { return m_Sprites.size(); }

    const Sprite* TryGetSprite(size_t index) const { return m_Sprites.TryGet(index); }

private:
    BoundedVector<Sprite> m_Sprites;
};

class SpriteAnim
{
public:
    struct Descriptor
    {
        uint32_t StatesPerDirection;
        uint32_t DirectionCount;
    };

    SpriteAnim() = delete;

    static Result<SpriteAnim> Create(const SpriteCollection& sprites, const Descriptor& desc);

    /// Returns the number of animation states per direction.
    uint32_t GetStatesPerDirection() const { return m_StatesPerDirection; }

    /// Returns the number of directions in the animation.
    uint32_t GetDirectionCount() const { return m_DirectionCount; }

    /// Returns the sprite for the given animation state and direction.
    /// The state should be in the range [0, StatesPerDirection) and the direction should be in the
    /// range [0, DirectionCount).
    Result<const Sprite*> GetFrame(const uint32_t state, const uint32_t direction) const;

private:
    SpriteAnim(const SpriteCollection& sprites, const Descriptor& desc);

    const SpriteCollection* m_Sprites;
    uint32_t m_StatesPerDirection;
    uint32_t m_DirectionCount;
};
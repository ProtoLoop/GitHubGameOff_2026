#pragma once

#include "BoundedVector.h"
#include "Sprite.h"
#include "TextureLoader.h"

#include <span>

class Renderer;
class WebGpu;

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
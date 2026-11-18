#pragma once

#include "BoundedVector.h"
#include "Sprite.h"
#include "TextureLoader.h"

class WebGpu;
class Renderer;

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
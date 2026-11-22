#pragma once

#include "BoundedVector.h"
#include "Sprite.h"
#include "TextureLoader.h"

#include <span>

class Renderer;
class WebGpu;

class SpriteSequenceLoader
{
public:
    SpriteSequenceLoader() = delete;
    ~SpriteSequenceLoader() = default;
    SpriteSequenceLoader(const SpriteSequenceLoader&) = delete;
    SpriteSequenceLoader& operator=(const SpriteSequenceLoader&) = delete;
    SpriteSequenceLoader(SpriteSequenceLoader&&) = default;
    SpriteSequenceLoader& operator=(SpriteSequenceLoader&&) = default;

    static Result<SpriteSequenceLoader> Load(const std::span<const SpriteFrameSequenceDef> spriteSeqDefs,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<BoundedVector<SpriteFrameSequence>> Take();

private:
    SpriteSequenceLoader(BoundedVector<TextureLoader>&& textureLoaders,
        BoundedVector<size_t>&& sequenceToTextureMap,
        BoundedVector<SpriteFrameSequence>&& spriteSequences,
        Renderer& renderer);

    BoundedVector<TextureLoader> m_TextureLoaders;
    BoundedVector<size_t> m_SequenceToTextureMap;
    BoundedVector<SpriteFrameSequence> m_SpriteSequences;
    Renderer* m_Renderer;

    bool m_Taken{ false };
};
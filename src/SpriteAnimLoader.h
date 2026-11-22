#pragma once

#include "Sprite.h"
#include "SpriteSequenceLoader.h"

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
    SpriteAnimLoader(const ResourceName& name, SpriteSequenceLoader&& spriteSeqLoader);

    ResourceName m_Name;

    SpriteSequenceLoader m_SpriteSeqLoader;
};
#pragma once

#include "AudioClipLoader.h"
#include "CellMap.h"
#include "ResourceDefs.h"
#include "Sprite.h"
#include "SpriteSequenceLoader.h"

#include <memory>

class Level;
class Prop;

class LevelLoader
{
public:
    LevelLoader() = delete;
    ~LevelLoader() = default;
    LevelLoader(const LevelLoader&) = delete;
    LevelLoader& operator=(const LevelLoader&) = delete;
    LevelLoader(LevelLoader&&) = default;
    LevelLoader& operator=(LevelLoader&&) = default;

    static Result<LevelLoader> Load(const LevelDef& levelDef,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu,
        AudioPlayer& player);

    bool IsPending() const;

    Result<> Update();

    Result<std::unique_ptr<Level>> Take();

private:
    LevelLoader(CellMapLoader&& cellMapLoader,
        SpriteSequenceLoader&& spriteSeqLoader,
        BoundedVector<AudioClipLoader>&& audioClipLoaders,
        BoundedVector<SpriteDef>&& spriteDefs,
        BoundedVector<PropDef>&& propDefs)
        : m_CellMapLoader(std::move(cellMapLoader)),
          m_SpriteSeqLoader(std::move(spriteSeqLoader)),
          m_AudioClipLoaders(std::move(audioClipLoaders)),
          m_SpriteDefs(std::move(spriteDefs)),
          m_PropDefs(std::move(propDefs))
    {
    }

    Result<BoundedVector<Sprite>> BuildSprites(
        const std::span<const SpriteFrameSequence> spriteFrameSeqs) const;

    Result<BoundedVector<Prop>> BuildProps(const std::span<const Sprite> sprites) const;

    Result<BoundedVector<AudioClip>> BuildAudioClips();

    CellMapLoader m_CellMapLoader;
    SpriteSequenceLoader m_SpriteSeqLoader;
    BoundedVector<AudioClipLoader> m_AudioClipLoaders;
    BoundedVector<SpriteDef> m_SpriteDefs;
    BoundedVector<PropDef> m_PropDefs;

    bool m_Taken{ false };
};
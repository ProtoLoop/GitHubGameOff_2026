#pragma once

#include "AudioClip.h"
#include "CellMap.h"
#include "Sprite.h"

#include <string_view>

/// Owns resources for the level.
class Level
{
public:
    Level() = delete;
    ~Level() = default;
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;
    Level(Level&&) = delete;
    Level& operator=(Level&&) = delete;

    const CellMap& GetCellMap() const;

    Result<const Sprite&> GetSprite(const std::string_view name) const;

    Result<Sprite::Instance> CreateSpriteInstance(const std::string_view name) const;

    Result<const AudioClip&> GetAudioClip(const std::string_view name) const;

private:
    friend class LevelLoader;

    Level(CellMap&& cellMap,
        BoundedVector<SpriteFrameSequence>&& spriteSeqs,
        BoundedVector<Sprite>&& sprites,
        BoundedVector<AudioClip>&& audioClips);

    CellMap m_CellMap;
    BoundedVector<SpriteFrameSequence> m_SpriteSeqs;
    BoundedVector<Sprite> m_Sprites;
    BoundedVector<AudioClip> m_AudioClips;
};

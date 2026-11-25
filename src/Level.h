#pragma once

#include "AudioClip.h"
#include "CellMap.h"
#include "Prop.h"
#include "Sprite.h"

#include <span>
#include <string_view>

class Level;

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

    Result<> SpawnProp(const std::string_view spriteName, const Transform2D& transform);

    std::span<const Prop> GetProps() const;

private:
    friend class LevelLoader;

    static constexpr size_t kMaxProps = 1024;

    Level(CellMap&& cellMap,
        BoundedVector<SpriteFrameSequence>&& spriteSeqs,
        BoundedVector<Sprite>&& sprites,
        BoundedVector<AudioClip>&& audioClips);

    CellMap m_CellMap;
    BoundedVector<SpriteFrameSequence> m_SpriteSeqs;
    BoundedVector<Sprite> m_Sprites;
    BoundedVector<AudioClip> m_AudioClips;
    InlineVector<Prop, kMaxProps> m_Props;
};

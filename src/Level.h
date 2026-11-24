#pragma once

#include "CellMap.h"
#include "Prop.h"
#include "Sprite.h"
#include "SpriteSequenceLoader.h"

#include <memory>
#include <span>
#include <string_view>

class Level;

struct PropDef
{
    uint32_t SpriteId{ 0 };
    Transform2D Transform;
};

struct LevelDef
{
    CellMapDef CellMapDef;
    std::span<const SpriteFrameSequenceDef> SpriteSeqDefs;
    std::span<const SpriteDef> SpriteDefs;
    std::span<const PropDef> PropDefs;
};

class LevelLoader
{
public:
    LevelLoader() = delete;
    ~LevelLoader() = default;
    LevelLoader(const LevelLoader&) = delete;
    LevelLoader& operator=(const LevelLoader&) = delete;
    LevelLoader(LevelLoader&&) = default;
    LevelLoader& operator=(LevelLoader&&) = default;

    static Result<LevelLoader> Load(
        const LevelDef& levelDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<std::unique_ptr<Level>> Take();

private:
    LevelLoader(CellMapLoader&& cellMapLoader,
        SpriteSequenceLoader&& spriteSeqLoader,
        BoundedVector<SpriteDef>&& spriteDefs,
        BoundedVector<PropDef>&& propDefs)
        : m_CellMapLoader(std::move(cellMapLoader)),
          m_SpriteSeqLoader(std::move(spriteSeqLoader)),
          m_SpriteDefs(std::move(spriteDefs)),
          m_PropDefs(std::move(propDefs))
    {
    }

    CellMapLoader m_CellMapLoader;
    SpriteSequenceLoader m_SpriteSeqLoader;
    BoundedVector<SpriteDef> m_SpriteDefs;
    BoundedVector<PropDef> m_PropDefs;

    bool m_Taken{ false };
};

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

    std::span<const Prop> GetProps() const;

private:
    friend class LevelLoader;

    Level(CellMap&& cellMap,
        BoundedVector<SpriteFrameSequence>&& spriteSeqs,
        BoundedVector<Sprite>&& sprites,
        BoundedVector<Prop>&& props);

    CellMap m_CellMap;
    BoundedVector<SpriteFrameSequence> m_SpriteSeqs;
    BoundedVector<Sprite> m_Sprites;
    BoundedVector<Prop> m_Props;
};

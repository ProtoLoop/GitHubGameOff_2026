#pragma once

#include "CellMap.h"
#include "Prop.h"
#include "Sprite.h"
#include "SpriteAnimLoader.h"
#include "SpriteLoader.h"

#include <memory>
#include <span>
#include <string_view>

class Level;

struct PropDef
{
    uint32_t SpriteId{0};
    Transform2D Transform;
};

struct LevelDef
{
    CellMapDef CellMapDef;
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

    static Result<LevelLoader> Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
        const LevelDef& levelDef,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<std::unique_ptr<Level>> Take();

private:
    LevelLoader(CellMapLoader&& cellMapLoader,
        SpriteLoader&& spriteLoader,
        BoundedVector<SpriteAnimLoader>&& spriteAnimLoaders,
        BoundedVector<PropDef>&& propDefs)
        : m_CellMapLoader(std::move(cellMapLoader)),
          m_SpriteLoader(std::move(spriteLoader)),
          m_SpriteAnimLoaders(std::move(spriteAnimLoaders)),
          m_PropDefs(std::move(propDefs))
    {
    }

    CellMapLoader m_CellMapLoader;
    SpriteLoader m_SpriteLoader;
    BoundedVector<SpriteAnimLoader> m_SpriteAnimLoaders;
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

    Result<const SpriteAnim&> GetSpriteAnim(const std::string_view name) const;

    Result<SpriteAnim::Instance> CreateSpriteAnimInstance(const std::string_view name) const;

    std::span<const Prop> GetProps() const;

private:
    friend class LevelLoader;

    Level(CellMap&& cellMap,
        BoundedVector<NamedSprite>&& sprites,
        BoundedVector<SpriteAnim>&& spriteAnims,
        BoundedVector<Prop>&& props);

    CellMap m_CellMap;
    BoundedVector<NamedSprite> m_Sprites;
    BoundedVector<SpriteAnim> m_SpriteAnims;
    BoundedVector<Prop> m_Props;
};

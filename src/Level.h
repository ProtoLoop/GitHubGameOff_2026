#pragma once

#include "CellMap.h"
#include "Sprite.h"
#include "SpriteAnimLoader.h"
#include "SpriteLoader.h"

#include <memory>
#include <span>
#include <string_view>

class Level;

class LevelLoader
{
public:
    LevelLoader() = delete;
    ~LevelLoader() = default;
    LevelLoader(const LevelLoader&) = delete;
    LevelLoader& operator=(const LevelLoader&) = delete;
    LevelLoader(LevelLoader&&) = default;
    LevelLoader& operator=(LevelLoader&&) = default;

    static Result<LevelLoader> Load(const std::span<const SpriteDef> spriteDefs,
        const std::span<const SpriteAnimDef> spriteAnimDefs,
        const CellMapDef& cellMapDef,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<std::unique_ptr<Level>> Take();

private:
    LevelLoader(CellMapLoader&& cellMapLoader,
        SpriteLoader&& spriteLoader,
        BoundedVector<SpriteAnimLoader>&& spriteAnimLoaders)
        : m_CellMapLoader(std::move(cellMapLoader)),
          m_SpriteLoader(std::move(spriteLoader)),
          m_SpriteAnimLoaders(std::move(spriteAnimLoaders))
    {
    }

    CellMapLoader m_CellMapLoader;
    SpriteLoader m_SpriteLoader;
    BoundedVector<SpriteAnimLoader> m_SpriteAnimLoaders;

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

private:

    friend class LevelLoader;

    Level(CellMap&& cellMap,
        BoundedVector<NamedSprite>&& sprites,
        BoundedVector<SpriteAnim>&& spriteAnims);

    CellMap m_CellMap;
    BoundedVector<NamedSprite> m_Sprites;
    BoundedVector<SpriteAnim> m_SpriteAnims;
};

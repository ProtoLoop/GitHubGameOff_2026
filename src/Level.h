#pragma once

#include "CellMap.h"
#include "Nodes.h"
#include "Sprite.h"

#include <initializer_list>
#include <span>
#include <string_view>

struct SpriteDrawRecord;
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

    static Result<LevelLoader> Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
        const CellMapDef& cellMapDef,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<Level> Take();

private:
    LevelLoader(CellMapLoader&& cellMapLoader, BoundedVector<SpriteAnimLoader>&& spriteAnimLoaders)
        : m_CellMapLoader(std::move(cellMapLoader)),
          m_SpriteAnimLoaders(std::move(spriteAnimLoaders))
    {
    }

    CellMapLoader m_CellMapLoader;
    BoundedVector<SpriteAnimLoader> m_SpriteAnimLoaders;

    bool m_Taken{ false };
};

class Level
{
public:
    static constexpr size_t kMaxNodes = 1024;
    static constexpr size_t kMaxSpriteAnimNodes = 1024;

    Level() = delete;
    ~Level() = default;
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;
    Level(Level&&) = default;
    Level& operator=(Level&&) = default;

    void Tick(const float deltaTime);

    const CellMap& GetCellMap() const;

    Result<const SpriteAnim&> GetSpriteAnim(const std::string_view name) const;

    Result<SpriteAnimNode&> CreateSpriteAnimNode(const std::initializer_list<std::string_view> names);

    Result<SpriteAnimNode&> CreateSpriteAnimNode(const std::span<const std::string_view> animNames);

    Result<SpriteAnim::Instance> CreateSpriteAnimInstance(const std::string_view name) const;

    std::span<const SpriteAnimNode> GetSpriteAnimNodes() const;

private:

    friend class LevelLoader;

    CellMap m_CellMap;
    BoundedVector<SpriteAnim> m_SpriteAnims;
    BoundedVector<Node2D> m_Nodes{kMaxNodes};
    BoundedVector<SpriteAnimNode> m_SpriteAnimNodes{kMaxSpriteAnimNodes};

    Level(CellMap&& cellMap, BoundedVector<SpriteAnim>&& spriteAnims);
};
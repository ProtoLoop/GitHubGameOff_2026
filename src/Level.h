#pragma once

#include "CellMap.h"
#include "Sprite.h"

#include <string_view>
#include <variant>

class Level
{
public:
    static Result<Level> Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
        const CellMapDef& cellMapDef,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    Level() = delete;
    ~Level() = default;
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;
    Level(Level&&) = default;
    Level& operator=(Level&&) = default;

    bool IsPending() const;

    Result<> Update();

    Result<const CellMap&> GetCellMap() const;

    Result<const SpriteAnim&> GetSpriteAnim(const std::string_view name) const;

private:
    struct LoadingState
    {
        CellMap CellMap;
        BoundedVector<SpriteAnim> SpriteAnims;
    };

    struct LoadedState
    {
        CellMap CellMap;
        BoundedVector<SpriteAnim> SpriteAnims;
    };

    explicit Level(LoadingState&& loadingState)
        : m_State(std::move(loadingState))
    {
    }

    struct FailedState
    {
    };
    std::variant<FailedState, LoadingState, LoadedState> m_State;
};
#pragma once

#include "TextureLoader.h"

#include <webgpu/webgpu_cpp.h>
#include <variant>

class Renderer;

class CellMap
{
public:
    /// Loads a cell map.
    /// Each path in tileset paths corresponds to the texture used for a layer of the map.
    static Result<CellMap> Load(const std::span<const FilePath> tileSetPaths,
        const uint32_t cellsPerSide,
        Renderer& renderer,
        FileFetcher& fileFetcher,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    /// Returns the bind group for the map's tile set texture.
    Result<wgpu::BindGroup> GetTileSetBindGroup() const;

    uint32_t GetCellsPerSide() const;

    uint32_t GetCellCount() const
    {
        return GetCellsPerSide() * GetCellsPerSide();
    }

private:
    struct LoadingState
    {
        TextureLoader m_TextureLoader;
        Renderer* m_Renderer{ nullptr };
        uint32_t m_CellsPerSide{ 0 };
    };

    struct LoadedState
    {
        wgpu::BindGroup m_TileSetBindGroup;
        uint32_t m_CellsPerSide{ 0 };
    };

    explicit CellMap(LoadingState&& state);

    std::variant<std::monostate, LoadingState, LoadedState> m_State;
};
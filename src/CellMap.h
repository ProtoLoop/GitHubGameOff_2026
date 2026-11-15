#pragma once

#include "TextureLoader.h"

#include <webgpu/webgpu_cpp.h>

class CellMap;
class FileFetcher;
class Renderer;
class WebGpu;

struct CellMapDef
{
    uint32_t CellsPerSide{ 0 };
    std::vector<FilePath> TileSets;
};

class CellMapLoader
{
public:
    /// Loads a cell map.
    /// Each path in tileset paths corresponds to the texture used for a layer of the map.
    static Result<CellMapLoader> Load(
        const CellMapDef& cellMapDef, Renderer& renderer, FileFetcher& fileFetcher, WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<CellMap> Take();

private:
    CellMapLoader(TextureLoader&& textureLoader, Renderer& renderer, uint32_t cellsPerSide);

    TextureLoader m_TextureLoader;
    Renderer* m_Renderer{ nullptr };
    uint32_t m_CellsPerSide{ 0 };
    bool m_Taken{ false };
};

class CellMap
{
public:

    /// Returns the bind group for the map's tile set texture.
    wgpu::BindGroup GetTileSetBindGroup() const;

    uint32_t GetCellsPerSide() const;

    uint32_t GetCellCount() const
    {
        return GetCellsPerSide() * GetCellsPerSide();
    }

private:

    friend class CellMapLoader;

    CellMap(wgpu::BindGroup m_TileSetBindGroup, const uint32_t cellsPerSide);

    wgpu::BindGroup m_TileSetBindGroup;
    uint32_t m_CellsPerSide{ 0 };
};
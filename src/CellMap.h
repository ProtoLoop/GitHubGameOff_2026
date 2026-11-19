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
    std::span<const FilePath> TileSets;
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
    Renderer* m_Renderer;
    uint32_t m_CellsPerSide{ 0 };
    bool m_Taken{ false };
};

class CellMap
{
public:

    /// Returns the bind group for the map's tile set texture.
    wgpu::BindGroup GetTileSetBindGroup() const;

    /// Returns the length of one side of the cell map.
    uint32_t GetCellsPerSide() const;

    /// Returns the number of cells in a single layer of the cell map.
    uint32_t GetCellsPerLayer() const;

    /// Returns the number of layers in the cell map.
    uint32_t GetLayerCount() const;

private:
    friend class CellMapLoader;

    CellMap(wgpu::Texture tileSetTexture,
        wgpu::BindGroup m_TileSetBindGroup,
        const uint32_t cellsPerSide);

    wgpu::Texture m_TileSetTexture;
    wgpu::BindGroup m_TileSetBindGroup;
    uint32_t m_CellsPerSide{ 0 };
};
#pragma once

#include "Result.h"

#include <webgpu/webgpu_cpp.h>

class CellMap
{
public:
    static Result<std::unique_ptr<CellMap>> Create(const wgpu::Texture tileSets, const uint32_t gridSize);

    uint32_t GetGridSize() const { return m_GridSize; }
    wgpu::Texture GetTileSetTexture() const { return m_TileSets; }

private:
    CellMap(const wgpu::Texture tileSets, const uint32_t gridSize);

    wgpu::Texture m_TileSets;
    uint32_t m_GridSize;
};
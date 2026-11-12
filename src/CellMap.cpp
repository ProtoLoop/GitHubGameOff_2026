#include "CellMap.h"

Result<std::unique_ptr<CellMap>>
CellMap::Create(const wgpu::Texture tileSets, const uint32_t gridSize)
{
    return std::unique_ptr<CellMap>(new CellMap(tileSets, gridSize));
}

CellMap::CellMap(const wgpu::Texture tileSets, const uint32_t gridSize)
    : m_TileSets(tileSets),
      m_GridSize(gridSize)
{
}
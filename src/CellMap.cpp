#include "CellMap.h"

Result<CellMap>
CellMap::Create(const wgpu::Texture tileSets, const uint32_t gridSize)
{
    return CellMap(tileSets, gridSize);
}

CellMap::CellMap(const wgpu::Texture tileSets, const uint32_t gridSize)
    : m_TileSets(tileSets),
      m_GridSize(gridSize)
{
}
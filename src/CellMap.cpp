#include "CellMap.h"

Result<CellMap>
CellMap::Create(const wgpu::Texture tileSets, const uint32_t rowCount, const uint32_t colCount)
{
    return CellMap(tileSets, rowCount, colCount);
}

CellMap::CellMap(const wgpu::Texture tileSets, const uint32_t rowCount, const uint32_t colCount)
    : m_TileSets(tileSets),
      m_RowCount(rowCount),
      m_ColCount(colCount)
{
}
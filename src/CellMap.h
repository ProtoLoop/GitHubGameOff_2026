#pragma once

#include "Result.h"

#include <webgpu/webgpu_cpp.h>

class CellMap
{
public:
    static Result<CellMap> Create(
        const wgpu::Texture tileSets, const uint32_t rowCount, const uint32_t colCount);

    uint32_t GetRowCount() const { return m_RowCount; }
    uint32_t GetColCount() const { return m_ColCount; }
    wgpu::Texture GetTileSetTexture() const { return m_TileSets; }

private:
    CellMap(const wgpu::Texture tileSets,
        const uint32_t rowCount,
        const uint32_t colCount);

    wgpu::Texture m_TileSets;
    uint32_t m_RowCount;
    uint32_t m_ColCount;
};
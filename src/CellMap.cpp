#include "CellMap.h"

#include "Renderer.h"

/// CellMapLoader

Result<CellMapLoader>
CellMapLoader::Load(
    const CellMapDef& cellMapDef, Renderer& renderer, FileFetcher& fileFetcher, WebGpu& webGpu)
{
    auto loaderResult = TextureLoader::Load(cellMapDef.TileSets, fileFetcher, webGpu, "Test");
    GG_CHECKB(loaderResult, "Failed to start texture load");

    return CellMapLoader(std::move(*loaderResult), renderer, cellMapDef.CellsPerSide);
}

CellMapLoader::CellMapLoader(
    TextureLoader&& textureLoader, Renderer& renderer, uint32_t cellsPerSide)
    : m_TextureLoader(std::move(textureLoader)),
      m_Renderer(&renderer),
      m_CellsPerSide(cellsPerSide)
{
}

bool
CellMapLoader::IsPending() const
{
    return m_TextureLoader.IsPending();
}

Result<>
CellMapLoader::Update()
{
    return m_TextureLoader.Update();
}

Result<CellMap>
CellMapLoader::Take()
{
    GG_CHECKB(!IsPending(), "Cell map is still loading");
    GG_CHECKB(!m_Taken, "Cell map has already been taken");
    m_Taken = true;

    auto textureResult = m_TextureLoader.Take();
    GG_CHECKB(textureResult, "Failed to load texture");

    auto bgResult = m_Renderer->CreateTileSetTextureBindGroup(*textureResult);
    GG_CHECK(bgResult, "Failed to create map texture bind group");

    return CellMap(std::move(*bgResult), m_CellsPerSide);

}

/// CellMap

CellMap::CellMap(wgpu::BindGroup m_TileSetBindGroup, const uint32_t cellsPerSide)
    : m_TileSetBindGroup(std::move(m_TileSetBindGroup)),
      m_CellsPerSide(cellsPerSide)
{
}

wgpu::BindGroup
CellMap::GetTileSetBindGroup() const
{
    return m_TileSetBindGroup;
}

uint32_t
CellMap::GetCellsPerSide() const
{
    return m_CellsPerSide;
}
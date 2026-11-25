#include "CellMap.h"

#include "Renderer.h"
#include "ResourceDefs.h"

namespace
{

/// Prunes the list of tile sheet paths to only include those that are actually used by the
/// layers.
Result<BoundedVector<FilePath>>
PruneTileSheets(const std::span<const FilePath> paths,
    const std::span<const CellMapLayerDef> layers)
{
    BoundedVector<FilePath> tileSheetPaths(layers.size());

    for(const CellMapLayerDef& layer : layers)
    {
        const uint32_t sheetIndex = layer.SheetIndex;
        GG_CHECKB(sheetIndex < paths.size(), "Invalid sheet index: {}", sheetIndex);

        const FilePath& tileSheetPath = paths[sheetIndex];
        tileSheetPaths.push_back(tileSheetPath);
    }
    return tileSheetPaths;
}
} // namespace

/// CellMapLoader

Result<CellMapLoader>
CellMapLoader::Load(
    const CellMapDef& cellMapDef, Renderer& renderer, FileFetcher& fileFetcher, WebGpu& webGpu)
{
    auto prunedTileSheetsResult = PruneTileSheets(cellMapDef.TileSheets, cellMapDef.Layers);
    GG_CHECKB(prunedTileSheetsResult, "Failed to prune tile sheets");

    // FIXME(KB) - deal with duplicate texture paths in the tile sheets.

    auto loaderResult = TextureLoader::Load(*prunedTileSheetsResult, fileFetcher, webGpu, "Test");
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

    return CellMap(std::move(*textureResult), std::move(*bgResult), m_CellsPerSide);
}

/// CellMap

CellMap::CellMap(
    wgpu::Texture tileSetTexture, wgpu::BindGroup m_TileSetBindGroup, const uint32_t cellsPerSide)
    : m_TileSetTexture(std::move(tileSetTexture)),
      m_TileSetBindGroup(std::move(m_TileSetBindGroup)),
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

uint32_t
CellMap::GetCellsPerLayer() const
{
    return GetCellsPerSide() * GetCellsPerSide();
}

uint32_t
CellMap::GetLayerCount() const
{
    return m_TileSetTexture.GetDepthOrArrayLayers();
}
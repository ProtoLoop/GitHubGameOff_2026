#include "CellMap.h"

#include "Defer.h"
#include "Renderer.h"

/// TileSet

Result<TileSet>
TileSet::Load(const std::span<const FilePath> tileSetPaths,
    const uint32_t tilesPerSide,
    Renderer& renderer,
    FileFetcher& fileFetcher,
    WebGpu& webGpu)
{
    auto loaderResult = TextureLoader::Load(tileSetPaths, fileFetcher, webGpu, "Test");
    GG_CHECKB(loaderResult, "Failed to start texture load");

    return TileSet(LoadingState //
        {
            .m_TextureLoader = std::move(*loaderResult),
            .m_Renderer = &renderer,
            .m_TilesPerSide = tilesPerSide,
        });
}

TileSet::TileSet(LoadingState&& state)
    : m_State(std::move(state))
{
}

Result<>
TileSet::Update()
{
    if(std::holds_alternative<LoadingState>(m_State))
    {
        auto& state = std::get<LoadingState>(m_State);

        if(state.m_TextureLoader.IsPending())
        {
            return Result<>::Ok;
        }

        auto onFailed = GG_DEFER
        {
            if(std::holds_alternative<LoadingState>(m_State))
            {
                m_State = FailedState{};
            }
        };

        auto textureResult = state.m_TextureLoader.Take();
        GG_CHECKB(textureResult, "Failed to load texture");

        auto bgResult = state.m_Renderer->CreateTileSetTextureBindGroup(*textureResult);
        GG_CHECK(bgResult, "Failed to create map texture bind group");

        m_State = LoadedState //
            {
                .m_BindGroup = std::move(*bgResult),
                .m_TilesPerSide = state.m_TilesPerSide,
            };
    }
    return Result<>::Ok;
}

Result<wgpu::BindGroup>
TileSet::GetBindGroup() const
{
    GG_CHECKB(std::holds_alternative<LoadedState>(m_State), "Tile set not loaded");

    return std::get<LoadedState>(m_State).m_BindGroup;
}

uint32_t
TileSet::GetTilesPerSide() const
{
    if(std::holds_alternative<LoadedState>(m_State))
    {
        return std::get<LoadedState>(m_State).m_TilesPerSide;
    }
    if(std::holds_alternative<LoadingState>(m_State))
    {
        return std::get<LoadingState>(m_State).m_TilesPerSide;
    }
    return 0;
}

bool
TileSet::IsPending() const
{
    return std::holds_alternative<LoadingState>(m_State);
}

/// CellMap

Result<CellMap>
CellMap::Load(const std::span<const FilePath> tileSetPaths,
    const uint32_t cellsPerSide,
    Renderer& renderer,
    FileFetcher& fileFetcher,
    WebGpu& webGpu)
{
    auto loaderResult = TextureLoader::Load(tileSetPaths, fileFetcher, webGpu, "Test");
    GG_CHECKB(loaderResult, "Failed to start texture load");

    return CellMap(LoadingState //
        {
            .m_TextureLoader = std::move(*loaderResult),
            .m_Renderer = &renderer,
            .m_CellsPerSide = cellsPerSide,
        });
}

CellMap::CellMap(LoadingState&& state)
    : m_State(std::move(state))
{
}

Result<>
CellMap::Update()
{
    if(std::holds_alternative<LoadingState>(m_State))
    {
        auto& state = std::get<LoadingState>(m_State);

        if(state.m_TextureLoader.IsPending())
        {
            return Result<>::Ok;
        }

        auto onFailed = GG_DEFER
        {
            if(std::holds_alternative<LoadingState>(m_State))
            {
                m_State = FailedState{};
            }
        };

        auto textureResult = state.m_TextureLoader.Take();
        GG_CHECKB(textureResult, "Failed to load texture");

        auto bgResult = state.m_Renderer->CreateTileSetTextureBindGroup(*textureResult);
        GG_CHECK(bgResult, "Failed to create map texture bind group");

        m_State = LoadedState //
            {
                .m_TileSetBindGroup = std::move(*bgResult),
                .m_CellsPerSide = state.m_CellsPerSide,
            };
    }
    return Result<>::Ok;
}

Result<wgpu::BindGroup>
CellMap::GetTileSetBindGroup() const
{
    GG_CHECKB(std::holds_alternative<LoadedState>(m_State), "Cell map not loaded");

    return std::get<LoadedState>(m_State).m_TileSetBindGroup;
}

uint32_t
CellMap::GetCellsPerSide() const
{
    if(std::holds_alternative<LoadedState>(m_State))
    {
        return std::get<LoadedState>(m_State).m_CellsPerSide;
    }
    if(std::holds_alternative<LoadingState>(m_State))
    {
        return std::get<LoadingState>(m_State).m_CellsPerSide;
    }
    return 0;
}

bool
CellMap::IsPending() const
{
    return std::holds_alternative<LoadingState>(m_State);
}
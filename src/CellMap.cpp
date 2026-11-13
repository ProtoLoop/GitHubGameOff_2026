#include "CellMap.h"

#include "Defer.h"
#include "Renderer.h"

Result<CellMap>
CellMap::Load(const std::span<const FilePath> tileSetPaths,
    const uint32_t cellsPerSide,
    Renderer& renderer,
    FileFetcher& fileFetcher,
    WebGpu& webGpu)
{
    auto loaderResult =
        TextureLoader::Load(tileSetPaths, fileFetcher, webGpu, "Test");
    GG_CHECKB(loaderResult, "Failed to start texture load");
    
    return CellMap(LoadingState//
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
            return Result<>::Success();
        }

        auto onFailed = GG_DEFER
        {
            if(std::holds_alternative<LoadingState>(m_State))
            {
                m_State = std::monostate{};
            }
        };

        auto textureResult = state.m_TextureLoader.Take();
        GG_CHECKB(textureResult, "Failed to load texture");

        auto bgResult = state.m_Renderer->CreateTileSetTextureBindGroup(*textureResult);
        GG_CHECK(bgResult, "Failed to create map texture bind group");

        m_State = LoadedState//
        {
            .m_TileSetBindGroup = std::move(*bgResult),
            .m_CellsPerSide = state.m_CellsPerSide,
        };
    }
    return Result<>::Success();
}

Result<wgpu::BindGroup> CellMap::GetTileSetBindGroup() const
{
    GG_CHECKB(std::holds_alternative<LoadedState>(m_State), "Cell map not loaded");

    return std::get<LoadedState>(m_State).m_TileSetBindGroup;
}

uint32_t CellMap::GetCellsPerSide() const
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
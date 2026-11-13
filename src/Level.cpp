#include "Level.h"

#include "Defer.h"
#include "Sprite.h"
#include <variant>

Result<Level>
Level::Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
    const CellMapDef& cellMapDef,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu)
{
    BoundedVector<SpriteAnim> spriteAnims(spriteAnimDefs.size());

    for(std::span s = spriteAnimDefs; !s.empty(); s = s.subspan(1))
    {
        const SpriteAnimDef& def = s.front();

        // Check for duplicate names.
        for(const SpriteAnimDef& def2 : s.subspan(1))
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate sprite anim name: {}", def.Name);
        }

        auto loadResult = SpriteAnim::Load(def, fileFetcher, renderer, webGpu);

        GG_CHECK(loadResult, "Failed to load sprite anim: {}", def.Name);

        GG_CHECK(spriteAnims.push_back(std::move(*loadResult)),
            "Failed to add sprite anim: {}",
            def.Name);
    }

    auto cellMapLoadResult = CellMap::Load(cellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellMapLoadResult, "Failed to load cell map");

    return Level(LoadingState //
        {
            .CellMap = std::move(*cellMapLoadResult),
            .SpriteAnims = std::move(spriteAnims),
        });
}

bool
Level::IsPending() const
{
    return std::holds_alternative<LoadingState>(m_State);
}

Result<>
Level::Update()
{
    auto onFail = GG_DEFER
    {
        m_State = FailedState{};
    };

    if(std::holds_alternative<LoadingState>(m_State))
    {
        auto& state = std::get<LoadingState>(m_State);

        bool pending = false;

        if(state.CellMap.IsPending())
        {
            GG_CHECK(state.CellMap.Update(), "Failed to update cell map");
            pending = true;
        }

        for(SpriteAnim& anim : state.SpriteAnims)
        {
            if(!anim.IsPending())
            {
                continue;
            }

            GG_CHECK(anim.Update(), "Failed to update sprite anim");
            pending = true;
        }

        if(!pending)
        {
            m_State = LoadedState //
                {
                    .CellMap = std::move(state.CellMap),
                    .SpriteAnims = std::move(state.SpriteAnims),
                };
        }
    }

    onFail.cancel();

    return Result<>::Ok;
}

Result<const CellMap&>
Level::GetCellMap() const
{
    GG_CHECK(std::holds_alternative<LoadedState>(m_State), "Level is not loaded");
    return std::get<LoadedState>(m_State).CellMap;
}

Result<const SpriteAnim&>
Level::GetSpriteAnim(const std::string_view name) const
{
    GG_CHECK(std::holds_alternative<LoadedState>(m_State), "Level is not loaded");
    const auto& state = std::get<LoadedState>(m_State);

    for(const SpriteAnim& anim : state.SpriteAnims)
    {
        if(anim.GetName() == name)
        {
            return anim;
        }
    }

    return Result<>::Error;
}
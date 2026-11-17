#include "Level.h"

#include "Renderer.h"
#include "Sprite.h"

/// LevelLoader

Result<LevelLoader>
LevelLoader::Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
    const CellMapDef& cellMapDef,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu)
{
    BoundedVector<SpriteAnimLoader> spriteAnimLoaders(spriteAnimDefs.size());

    for(std::span s = spriteAnimDefs; !s.empty(); s = s.subspan(1))
    {
        const SpriteAnimDef& def = s.front();

        // Check for duplicate names.
        for(const SpriteAnimDef& def2 : s.subspan(1))
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate sprite anim name: {}", def.Name);
        }

        auto loadResult = SpriteAnimLoader::Load(def, fileFetcher, renderer, webGpu);

        GG_CHECK(loadResult, "Failed to load sprite anim: {}", def.Name);

        spriteAnimLoaders.push_back(std::move(*loadResult));
    }

    auto cellResult = CellMapLoader::Load(cellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellResult, "Failed to load cell map");

    return LevelLoader(std::move(*cellResult), std::move(spriteAnimLoaders));
}

bool
LevelLoader::IsPending() const
{
    if(m_CellMapLoader.IsPending())
    {
        return true;
    }
    for(const SpriteAnimLoader& loader : m_SpriteAnimLoaders)
    {
        if(loader.IsPending())
        {
            return true;
        }
    }
    return false;
}

Result<>
LevelLoader::Update()
{
    if(m_CellMapLoader.IsPending())
    {
        GG_CHECK(m_CellMapLoader.Update(), "Failed to update cell map");
    }

    for(SpriteAnimLoader& loader : m_SpriteAnimLoaders)
    {
        if(!loader.IsPending())
        {
            continue;
        }

        GG_CHECK(loader.Update(), "Failed to update sprite anim");
    }

    return Result<>::Ok;
}

Result<std::unique_ptr<Level>>
LevelLoader::Take()
{
    GG_CHECKB(!IsPending(), "Level is still loading");
    GG_CHECKB(!m_Taken, "Level has already been taken");
    m_Taken = true;

    BoundedVector<SpriteAnim> spriteAnims(m_SpriteAnimLoaders.size());

    for(SpriteAnimLoader& loader : m_SpriteAnimLoaders)
    {
        auto animResult = loader.Take();
        GG_CHECK(animResult, "Failed to load sprite anim: {}", loader.GetName());
        spriteAnims.push_back(std::move(*animResult));
    }

    auto cellMapResult = m_CellMapLoader.Take();
    GG_CHECK(cellMapResult, "Failed to load cell map");

    return std::unique_ptr<Level>(new Level(std::move(*cellMapResult), std::move(spriteAnims)));
}

/// Level

Level::Level(CellMap&& cellMap, BoundedVector<SpriteAnim>&& spriteAnims)
    : m_CellMap(std::move(cellMap)),
      m_SpriteAnims(std::move(spriteAnims))
{
}

const CellMap&
Level::GetCellMap() const
{
    return m_CellMap;
}

Result<const SpriteAnim&>
Level::GetSpriteAnim(const std::string_view name) const
{
    const SpriteAnim* foundAnim = nullptr;
    for(const SpriteAnim& anim : m_SpriteAnims)
    {
        if(anim.GetName() == name)
        {
            foundAnim = &anim;
            break;
        }
    }

    GG_CHECKB(foundAnim, "Sprite anim not found: {}", name);

    return *foundAnim;
}

Result<SpriteAnim::Instance>
Level::CreateSpriteAnimInstance(const std::string_view name) const
{
    auto animResult = GetSpriteAnim(name);
    GG_CHECKB(animResult, "Failed to get sprite anim for {}", name);
    const auto& anim = *animResult;

    return anim.CreateInstance();
}
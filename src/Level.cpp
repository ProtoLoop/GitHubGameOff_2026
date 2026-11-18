#include "Level.h"

#include "Renderer.h"
#include "Sprite.h"

/// LevelLoader

Result<LevelLoader>
LevelLoader::Load(const std::span<const SpriteDef> spriteDefs,
    const std::span<const SpriteAnimDef> spriteAnimDefs,
    const CellMapDef& cellMapDef,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu)
{
    std::span spriteDefSubspan = spriteDefs;

    for(const SpriteDef& def : spriteDefs)
    {
        // Check for duplicate names.
        spriteDefSubspan = spriteDefSubspan.subspan(1);

        for(const SpriteDef& def2 : spriteDefSubspan)
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate sprite name: {}", def.Name);
        }
    }

    auto spriteLoadResult = SpriteLoader::Load(spriteDefs, fileFetcher, renderer, webGpu);

    GG_CHECK(spriteLoadResult, "Failed to load sprites");

    BoundedVector<SpriteAnimLoader> spriteAnimLoaders(spriteAnimDefs.size());

    std::span animDefSubspan = spriteAnimDefs;

    for(const SpriteAnimDef& def : spriteAnimDefs)
    {
        // Check for duplicate names.
        animDefSubspan = animDefSubspan.subspan(1);

        for(const SpriteAnimDef& def2 : animDefSubspan)
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate sprite anim name: {}", def.Name);
        }

        auto loadResult = SpriteAnimLoader::Load(def, fileFetcher, renderer, webGpu);

        GG_CHECK(loadResult, "Failed to load sprite anim: {}", def.Name);

        spriteAnimLoaders.push_back(std::move(*loadResult));
    }

    auto cellResult = CellMapLoader::Load(cellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellResult, "Failed to load cell map");

    return LevelLoader(std::move(*cellResult),
        std::move(*spriteLoadResult),
        std::move(spriteAnimLoaders));
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

    return m_SpriteLoader.IsPending();
}

Result<>
LevelLoader::Update()
{
    if(m_CellMapLoader.IsPending())
    {
        GG_CHECK(m_CellMapLoader.Update(), "Failed to update cell map loader");
    }
    
    if(m_SpriteLoader.IsPending())
    {
        GG_CHECK(m_SpriteLoader.Update(), "Failed to update sprite loader");
    }

    for(SpriteAnimLoader& loader : m_SpriteAnimLoaders)
    {
        if(loader.IsPending())
        {
            GG_CHECK(loader.Update(), "Failed to update sprite anim loader: {}", loader.GetName());
        }
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

    auto spritesResult = m_SpriteLoader.Take();
    GG_CHECK(spritesResult, "Failed to load sprites");

    return std::unique_ptr<Level>(
        new Level(std::move(*cellMapResult), std::move(*spritesResult), std::move(spriteAnims)));
}

/// Level

Level::Level(CellMap&& cellMap,
    BoundedVector<NamedSprite>&& sprites,
    BoundedVector<SpriteAnim>&& spriteAnims)
    : m_CellMap(std::move(cellMap)),
      m_Sprites(std::move(sprites)),
      m_SpriteAnims(std::move(spriteAnims))
{
}

const CellMap&
Level::GetCellMap() const
{
    return m_CellMap;
}

Result<const Sprite&>
Level::GetSprite(const std::string_view name) const
{
    const Sprite* foundSprite = nullptr;
    for(const NamedSprite& namedSprite : m_Sprites)
    {
        if(namedSprite.GetName() == name)
        {
            foundSprite = &namedSprite.GetSprite();
            break;
        }
    }

    GG_CHECKB(foundSprite, "Sprite not found: {}", name);

    return *foundSprite;
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
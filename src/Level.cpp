#include "Level.h"

#include "Renderer.h"
#include "Sprite.h"

/// LevelLoader

Result<LevelLoader>
LevelLoader::Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
    const LevelDef& levelDef,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu)
{
    std::span spriteDefSubspan = levelDef.SpriteSeqDefs;

    for(const SpriteFrameSequenceDef& def : levelDef.SpriteSeqDefs)
    {
        // Check for duplicate names.
        spriteDefSubspan = spriteDefSubspan.subspan(1);

        for(const SpriteFrameSequenceDef& def2 : spriteDefSubspan)
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate sprite name: {}", def.Name);
        }
    }

    auto spriteSeqLoadResult =
        SpriteSequenceLoader::Load(levelDef.SpriteSeqDefs, fileFetcher, renderer, webGpu);
    GG_CHECK(spriteSeqLoadResult, "Failed to load sprites");

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

    auto cellResult = CellMapLoader::Load(levelDef.CellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellResult, "Failed to load cell map");

    BoundedVector<PropDef> propDefs(levelDef.PropDefs.size());

    for(const PropDef& propDef : levelDef.PropDefs)
    {
        propDefs.push_back(propDef);
    }

    return LevelLoader(std::move(*cellResult),
        std::move(*spriteSeqLoadResult),
        std::move(spriteAnimLoaders),
        std::move(propDefs));
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

    return m_SpriteSeqLoader.IsPending();
}

Result<>
LevelLoader::Update()
{
    if(m_CellMapLoader.IsPending())
    {
        GG_CHECK(m_CellMapLoader.Update(), "Failed to update cell map loader");
    }
    
    if(m_SpriteSeqLoader.IsPending())
    {
        GG_CHECK(m_SpriteSeqLoader.Update(), "Failed to update sprite loader");
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

    auto spriteSeqsResult = m_SpriteSeqLoader.Take();
    GG_CHECK(spriteSeqsResult, "Failed to load sprite sequeneces");

    BoundedVector<Prop> props(m_PropDefs.size());

    for(const PropDef& propDef : m_PropDefs)
    {
        GG_CHECKB(propDef.SpriteId < spriteSeqsResult->size(), "Invalid sprite ID for prop");

        const SpriteFrameSequence& seq = (*spriteSeqsResult)[propDef.SpriteId];

        props.emplace_back(seq, propDef.Transform);
    }

    return std::unique_ptr<Level>(new Level(std::move(*cellMapResult),
        std::move(*spriteSeqsResult),
        std::move(spriteAnims),
        std::move(props)));
}

/// Level

Level::Level(CellMap&& cellMap,
    BoundedVector<SpriteFrameSequence>&& spriteSeqs,
    BoundedVector<SpriteAnim>&& spriteAnims,
    BoundedVector<Prop>&& props)
    : m_CellMap(std::move(cellMap)),
      m_SpriteSeqs(std::move(spriteSeqs)),
      m_SpriteAnims(std::move(spriteAnims)),
      m_Props(std::move(props))
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

std::span<const Prop>
Level::GetProps() const
{
    return m_Props;
}
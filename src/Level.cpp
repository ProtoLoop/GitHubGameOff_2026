#include "Level.h"

#include "Renderer.h"
#include "Sprite.h"

/// LevelLoader

Result<LevelLoader>
LevelLoader::Load(
    const LevelDef& levelDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu)
{
    // Check for duplicate sprite frame sequence names.

    std::span spriteSeqDefSubspan = levelDef.SpriteSeqDefs;

    for(const SpriteFrameSequenceDef& def : levelDef.SpriteSeqDefs)
    {
        spriteSeqDefSubspan = spriteSeqDefSubspan.subspan(1);

        for(const SpriteFrameSequenceDef& def2 : spriteSeqDefSubspan)
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate sprite sequence name: {}", def.Name);
        }
    }

    BoundedVector<SpriteDef> spriteDefs(levelDef.SpriteDefs.size());

    // Check for duplicate sprite names, and copy the sprite def array that
    // will be passed to the sprite loader.

    std::span spriteDefSubspan = levelDef.SpriteDefs;

    for(const SpriteDef& spriteDef : levelDef.SpriteDefs)
    {
        spriteDefSubspan = spriteDefSubspan.subspan(1);

        // Check for duplicate names.
        for(const SpriteDef& def2 : spriteDefSubspan)
        {
            GG_CHECKB(def2.Name != spriteDef.Name, "Duplicate sprite name: {}", spriteDef.Name);
        }

        spriteDefs.push_back(spriteDef);
    }

    // Load the sprite sequences.

    auto spriteSeqLoadResult =
        SpriteSequenceLoader::Load(levelDef.SpriteSeqDefs, fileFetcher, renderer, webGpu);
    GG_CHECK(spriteSeqLoadResult, "Failed to load sprites");

    auto cellResult = CellMapLoader::Load(levelDef.CellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellResult, "Failed to load cell map");

    // Copy the prop defs so it can be passed to the level loader.
    BoundedVector<PropDef> propDefs(levelDef.PropDefs.size());

    for(const PropDef& propDef : levelDef.PropDefs)
    {
        propDefs.push_back(propDef);
    }

    return LevelLoader(std::move(*cellResult),
        std::move(*spriteSeqLoadResult),
        std::move(spriteDefs),
        std::move(propDefs));
}

bool
LevelLoader::IsPending() const
{
    return m_CellMapLoader.IsPending() || m_SpriteSeqLoader.IsPending();
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

    return Result<>::Ok;
}

Result<std::unique_ptr<Level>>
LevelLoader::Take()
{
    GG_CHECKB(!IsPending(), "Level is still loading");
    GG_CHECKB(!m_Taken, "Level has already been taken");
    m_Taken = true;

    auto spriteSeqsResult = m_SpriteSeqLoader.Take();
    GG_CHECK(spriteSeqsResult, "Failed to load sprite sequeneces");

    // Create the sprites.
    BoundedVector<Sprite> sprites(m_SpriteDefs.size());

    for(const SpriteDef& spriteDef : m_SpriteDefs)
    {
        // Build the array of sprite frame sequences for this sprite.
        BoundedVector<const SpriteFrameSequence*> spriteSeqs(spriteDef.SequenceNames.size());

        // Find the sprite frame sequences for this sprite.
        for(const ResourceName& seqName : spriteDef.SequenceNames)
        {
            const SpriteFrameSequence* foundSeq = nullptr;
            for(const SpriteFrameSequence& seq : *spriteSeqsResult)
            {
                if(seq.GetName() == seqName)
                {
                    foundSeq = &seq;
                    break;
                }
            }
            GG_CHECKB(foundSeq, "Sprite sequence not found: {}", seqName);
            spriteSeqs.push_back(foundSeq);
        }

        GG_CHECKB(!spriteSeqs.empty(), "No sprite sequences found for sprite: {}", spriteDef.Name);

        Sprite sprite(spriteDef.Name, std::move(spriteSeqs));
        sprites.push_back(std::move(sprite));
    }

    auto cellMapResult = m_CellMapLoader.Take();
    GG_CHECK(cellMapResult, "Failed to load cell map");

    BoundedVector<Prop> props(m_PropDefs.size());

    for(const PropDef& propDef : m_PropDefs)
    {
        GG_CHECKB(propDef.SpriteId < sprites.size(), "Invalid sprite ID for prop");

        const Sprite& sprite = sprites[propDef.SpriteId];

        auto spriteInstanceResult = sprite.CreateInstance();
        GG_CHECK(spriteInstanceResult, "Failed to create sprite instance: {}", sprite.GetName());

        props.emplace_back(*spriteInstanceResult, propDef.Transform);
    }

    return std::unique_ptr<Level>(new Level(std::move(*cellMapResult),
        std::move(*spriteSeqsResult),
        std::move(sprites),
        std::move(props)));
}

/// Level

Level::Level(CellMap&& cellMap,
    BoundedVector<SpriteFrameSequence>&& spriteSeqs,
    BoundedVector<Sprite>&& sprites,
    BoundedVector<Prop>&& props)
    : m_CellMap(std::move(cellMap)),
      m_SpriteSeqs(std::move(spriteSeqs)),
      m_Sprites(std::move(sprites)),
      m_Props(std::move(props))
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
    for(const Sprite& sprite : m_Sprites)
    {
        if(sprite.GetName() == name)
        {
            foundSprite = &sprite;
            break;
        }
    }

    GG_CHECKB(foundSprite, "Sprite not found: {}", name);

    return *foundSprite;
}

Result<Sprite::Instance>
Level::CreateSpriteInstance(const std::string_view name) const
{
    auto spriteResult = GetSprite(name);
    GG_CHECKB(spriteResult, "Failed to get sprite for {}", name);
    const auto& sprite = *spriteResult;

    return sprite.CreateInstance();
}

std::span<const Prop>
Level::GetProps() const
{
    return m_Props;
}
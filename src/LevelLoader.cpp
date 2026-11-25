#include "LevelLoader.h"

#include "Level.h"
#include "Prop.h"
#include "Renderer.h"
#include "Sprite.h"

/// LevelLoader

Result<LevelLoader>
LevelLoader::Load(const LevelDef& levelDef,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu,
    AudioPlayer& player)
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

    // Load the cell map

    auto cellResult = CellMapLoader::Load(levelDef.CellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellResult, "Failed to load cell map");

    // Load the audio clips.
    BoundedVector<AudioClipLoader> audioClipLoaders(levelDef.AudioClipDefs.size());

    // Check for duplicate audio clip names and paths.
    std::span audioClipDefSubspan = levelDef.AudioClipDefs;
    for(const AudioClipDef& def : levelDef.AudioClipDefs)
    {
        audioClipDefSubspan = audioClipDefSubspan.subspan(1);

        for(const AudioClipDef& def2 : audioClipDefSubspan)
        {
            GG_CHECKB(def2.Name != def.Name, "Duplicate audio clip name: {}", def.Name);
            GG_CHECKB(def2.FilePath != def.FilePath, "Duplicate audio clip path: {}", def.FilePath);
        }
    }

    for(const AudioClipDef& audioClipDef : levelDef.AudioClipDefs)
    {
        auto audioClipLoadResult = AudioClipLoader::Load(audioClipDef, fileFetcher, player);
        GG_CHECK(audioClipLoadResult, "Failed to load audio clip");
        audioClipLoaders.push_back(std::move(*audioClipLoadResult));
    }

    // Copy the prop defs so it can be passed to the level loader.
    BoundedVector<PropDef> propDefs(levelDef.PropDefs.size());

    for(const PropDef& propDef : levelDef.PropDefs)
    {
        propDefs.push_back(propDef);
    }

    return LevelLoader(std::move(*cellResult),
        std::move(*spriteSeqLoadResult),
        std::move(audioClipLoaders),
        std::move(spriteDefs),
        std::move(propDefs));
}

bool
LevelLoader::IsPending() const
{
    if(m_CellMapLoader.IsPending() || m_SpriteSeqLoader.IsPending())
    {
        return true;
    }
    
    for(const AudioClipLoader& audioClipLoader : m_AudioClipLoaders)
    {
        if(audioClipLoader.IsPending())
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
        GG_CHECK(m_CellMapLoader.Update(), "Failed to update cell map loader");
    }

    if(m_SpriteSeqLoader.IsPending())
    {
        GG_CHECK(m_SpriteSeqLoader.Update(), "Failed to update sprite loader");
    }

    for(AudioClipLoader& audioClipLoader : m_AudioClipLoaders)
    {
        if(audioClipLoader.IsPending())
        {
            GG_CHECK(audioClipLoader.Update(), "Failed to update audio clip loader");
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

    auto spriteSeqsResult = m_SpriteSeqLoader.Take();
    GG_CHECK(spriteSeqsResult, "Failed to load sprite sequeneces");

    auto spritesResult = BuildSprites(*spriteSeqsResult);
    GG_CHECK(spritesResult, "Failed to build sprites");

    auto cellMapResult = m_CellMapLoader.Take();
    GG_CHECK(cellMapResult, "Failed to load cell map");

    auto propsResult = BuildProps(*spritesResult);
    GG_CHECK(propsResult, "Failed to build props");

    auto audioClipsResult = BuildAudioClips();
    GG_CHECK(audioClipsResult, "Failed to build audio clips");

    return std::unique_ptr<Level>(new Level(std::move(*cellMapResult),
        std::move(*spriteSeqsResult),
        std::move(*spritesResult),
        std::move(*propsResult),
        std::move(*audioClipsResult)));
}

//private:

Result<BoundedVector<Sprite>>
LevelLoader::BuildSprites(const std::span<const SpriteFrameSequence> spriteFrameSeqs) const
{
    BoundedVector<Sprite> sprites(m_SpriteDefs.size());

    for(const SpriteDef& spriteDef : m_SpriteDefs)
    {
        // Build the array of sprite frame sequences for this sprite.
        BoundedVector<const SpriteFrameSequence*> spriteSeqs(spriteDef.SequenceNames.size());

        // Find the sprite frame sequences for this sprite.
        for(const ResourceName& seqName : spriteDef.SequenceNames)
        {
            const SpriteFrameSequence* foundSeq = nullptr;
            for(const SpriteFrameSequence& seq : spriteFrameSeqs)
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
    return sprites;
}

Result<BoundedVector<Prop>>
LevelLoader::BuildProps(const std::span<const Sprite> sprites) const
{
    BoundedVector<Prop> props(m_PropDefs.size());

    for(const PropDef& propDef : m_PropDefs)
    {
        GG_CHECKB(propDef.SpriteId < sprites.size(), "Invalid sprite ID for prop");

        const Sprite& sprite = sprites[propDef.SpriteId];

        auto spriteInstanceResult = sprite.CreateInstance();
        GG_CHECK(spriteInstanceResult, "Failed to create sprite instance: {}", sprite.GetName());

        props.emplace_back(*spriteInstanceResult, propDef.Transform);
    }

    return props;
}

Result<BoundedVector<AudioClip>>
LevelLoader::BuildAudioClips()
{
    BoundedVector<AudioClip> audioClips(m_AudioClipLoaders.size());

    for(AudioClipLoader& audioClipLoader : m_AudioClipLoaders)
    {
        auto audioClipResult = audioClipLoader.Take();
        GG_CHECK(audioClipResult, "Failed to load audio clip");
        audioClips.push_back(std::move(*audioClipResult));
    }

    return audioClips;
}
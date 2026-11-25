#include "LevelLoader.h"

#include "Level.h"
#include "Prop.h"
#include "Renderer.h"
#include "Sprite.h"

namespace
{
Result<>
CheckForDuplicateSequenceNames(const std::span<const SpriteFrameSequenceDef>& spriteSeqDefs)
{
    std::span span2 = spriteSeqDefs;

    for(const SpriteFrameSequenceDef& def : spriteSeqDefs)
    {
        span2 = span2.subspan(1);

        const auto it = std::ranges::find(span2, def.Name, &SpriteFrameSequenceDef::Name);
        GG_CHECKB(it == span2.end(), "Duplicate sprite sequence name: {}", def.Name);
    }
    return Result<>::Ok;
}

Result<>
CheckForDuplicateSpriteNames(const std::span<const SpriteDef>& spriteDefs)
{
    std::span span2 = spriteDefs;

    for(const SpriteDef& def : spriteDefs)
    {
        span2 = span2.subspan(1);

        const auto it = std::ranges::find(span2, def.Name, &SpriteDef::Name);
        GG_CHECKB(it == span2.end(), "Duplicate sprite name: {}", def.Name);
    }
    return Result<>::Ok;
}

Result<>
CheckForDuplicateAudioClipNames(const std::span<const AudioClipDef>& audioClipDefs)
{
    std::span span2 = audioClipDefs;

    for(const AudioClipDef& def : audioClipDefs)
    {
        span2 = span2.subspan(1);

        const auto itName = std::ranges::find(span2, def.Name, &AudioClipDef::Name);
        GG_CHECKB(itName == span2.end(), "Duplicate audio clip name: {}", def.Name);

        const auto itPath = std::ranges::find(span2, def.FilePath, &AudioClipDef::FilePath);
        GG_CHECKB(itPath == span2.end(), "Duplicate audio clip path: {}", def.FilePath);
    }
    return Result<>::Ok;
}

/// Prunes the list of sprite frame sequences to only include those that are actually used by the
/// sprite definitions.
Result<BoundedVector<SpriteFrameSequenceDef>>
PruneSpriteSequences(const std::span<const SpriteFrameSequenceDef> seqDefs,
    const std::span<const SpriteDef> spriteDefs)
{
    BoundedVector<SpriteFrameSequenceDef> spriteSeqDefs(seqDefs.size());

    // Build the sprite sequence list from sprite sequences that are actually used, instead of
    // all the sequences listed in the file.
    for(const SpriteDef& spriteDef : spriteDefs)
    {
        // For each sequence referenced by the sprite find it in the list of sprite sequences.
        for(const ResourceName& seqName : spriteDef.SequenceNames)
        {
            auto itSeqDef = std::ranges::find(seqDefs, seqName, &SpriteFrameSequenceDef::Name);
            GG_CHECKB(itSeqDef != seqDefs.end(), "Missing sprite sequence: {}", seqName.c_str());

            // Now add the found sequence to the list of used sequences if it's not already there.

            const bool alreadyInList =
                std::ranges::contains(spriteSeqDefs, seqName, &SpriteFrameSequenceDef::Name);

            if(!alreadyInList)
            {
                spriteSeqDefs.push_back(*itSeqDef);
            }
        }
    }
    return spriteSeqDefs;
}
} // namespace

Result<LevelLoader>
LevelLoader::Load(const LevelDef& levelDef,
    FileFetcher& fileFetcher,
    Renderer& renderer,
    WebGpu& webGpu,
    AudioPlayer& player)
{
    // Check for duplicate sprite frame sequence names.

    GG_CHECK(CheckForDuplicateSequenceNames(levelDef.SpriteSeqDefs));

    GG_CHECK(CheckForDuplicateSpriteNames(levelDef.SpriteDefs));

    BoundedVector<SpriteDef> spriteDefs(levelDef.SpriteDefs.size());

    // Make a copy of the sprite defs that will be passed to the sprite loader.

    for(const SpriteDef& spriteDef : levelDef.SpriteDefs)
    {
        spriteDefs.push_back(spriteDef);
    }

    // Prune then load the sprite sequences.

    auto prunedSeqDefsResult = PruneSpriteSequences(levelDef.SpriteSeqDefs, spriteDefs);
    GG_CHECK(prunedSeqDefsResult, "Failed to prune sprite sequences");

    auto spriteSeqLoadResult =
        SpriteSequenceLoader::Load(*prunedSeqDefsResult, fileFetcher, renderer, webGpu);
    GG_CHECK(spriteSeqLoadResult, "Failed to load sprites");

    // Load the cell map

    auto cellResult = CellMapLoader::Load(levelDef.CellMapDef, renderer, fileFetcher, webGpu);
    GG_CHECK(cellResult, "Failed to load cell map");

    GG_CHECK(CheckForDuplicateAudioClipNames(levelDef.AudioClipDefs));

    // Load the audio clips.
    BoundedVector<AudioClipLoader> audioClipLoaders(levelDef.AudioClipDefs.size());

    for(const AudioClipDef& audioClipDef : levelDef.AudioClipDefs)
    {
        auto audioClipLoadResult = AudioClipLoader::Load(audioClipDef, fileFetcher, player);
        GG_CHECK(audioClipLoadResult, "Failed to load audio clip");
        audioClipLoaders.push_back(std::move(*audioClipLoadResult));
    }

    return LevelLoader(std::move(*cellResult),
        std::move(*spriteSeqLoadResult),
        std::move(audioClipLoaders),
        std::move(spriteDefs));
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

    auto audioClipsResult = BuildAudioClips();
    GG_CHECK(audioClipsResult, "Failed to build audio clips");

    return std::unique_ptr<Level>(new Level(std::move(*cellMapResult),
        std::move(*spriteSeqsResult),
        std::move(*spritesResult),
        std::move(*audioClipsResult)));
}

// private:

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
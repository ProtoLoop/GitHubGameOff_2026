#include "SpriteAnimLoader.h"

SpriteAnimLoader::SpriteAnimLoader(const ResourceName& name, SpriteSequenceLoader&& spriteSeqLoader)
    : m_Name(name),
      m_SpriteSeqLoader(std::move(spriteSeqLoader))
{
}

Result<SpriteAnimLoader>
SpriteAnimLoader::Load(
    const SpriteAnimDef& animDef, FileFetcher& fileFetcher, Renderer& renderer, WebGpu& webGpu)
{
    auto spriteSeqLoaderResult =
        SpriteSequenceLoader::Load(animDef.SequenceDefs, fileFetcher, renderer, webGpu);
    GG_CHECKB(spriteSeqLoaderResult, "Failed to load sprite sequences: {}", animDef.Name);

    return SpriteAnimLoader(animDef.Name, std::move(*spriteSeqLoaderResult));
}

const ResourceName&
SpriteAnimLoader::GetName() const
{
    return m_Name;
}

bool
SpriteAnimLoader::IsPending() const
{
    return m_SpriteSeqLoader.IsPending();
}

Result<>
SpriteAnimLoader::Update()
{
    return m_SpriteSeqLoader.Update();
}

Result<SpriteAnim>
SpriteAnimLoader::Take()
{
    auto spriteSeqResults = m_SpriteSeqLoader.Take();
    GG_CHECKB(spriteSeqResults, "Failed to load sprite sequences");

    return SpriteAnim(m_Name, std::move(*spriteSeqResults));
}
#include "Level.h"

#include "Renderer.h"
#include "Sprite.h"

Level::Level(CellMap&& cellMap,
    BoundedVector<SpriteFrameSequence>&& spriteSeqs,
    BoundedVector<Sprite>&& sprites,
    BoundedVector<Prop>&& props,
    BoundedVector<AudioClip>&& audioClips)
    : m_CellMap(std::move(cellMap)),
      m_SpriteSeqs(std::move(spriteSeqs)),
      m_Sprites(std::move(sprites)),
      m_Props(std::move(props)),
      m_AudioClips(std::move(audioClips))
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


Result<const AudioClip&>
Level::GetAudioClip(const std::string_view name) const
{
    const AudioClip* foundAudioClip = nullptr;
    for(const AudioClip& audioClip : m_AudioClips)
    {
        if(audioClip.GetName() == name)
        {
            foundAudioClip = &audioClip;
            break;
        }
    }

    GG_CHECKB(foundAudioClip, "Audio clip not found: {}", name);

    return *foundAudioClip;
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
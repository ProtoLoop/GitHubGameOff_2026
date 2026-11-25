#pragma once

#include "AudioClip.h"
#include "FileFetcher.h"
#include "Result.h"

#include <cstddef>

class AudioPlayer;
struct AudioClipDef;

class AudioClipLoader
{
public:
    static constexpr size_t kMaxArrayLayers = 16;

    /// Loads an audio clip from the given path.
    static Result<AudioClipLoader> Load(
        const AudioClipDef& audioClipDef, FileFetcher& fileFetcher, AudioPlayer& player);

    bool IsPending() const;

    Result<> Update();

    Result<AudioClip> Take();

private:
    AudioClipLoader(const ResourceName& name, FetchRequestPtr fetchRequest, AudioPlayer& player)
        : m_Name(name),
          m_FetchRequest(std::move(fetchRequest)),
          m_Player(&player)
    {
    }

    ResourceName m_Name;
    FetchRequestPtr m_FetchRequest;
    AudioPlayer* m_Player;

    bool m_Taken{ false };
};
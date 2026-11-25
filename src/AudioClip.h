#pragma once

#include "NameTypes.h"

#include <memory>

struct MIX_Audio;

class AudioClip
{
public:
    AudioClip() = delete;
    ~AudioClip() = default;
    AudioClip(const AudioClip&) = delete;
    AudioClip& operator=(const AudioClip&) = delete;
    AudioClip(AudioClip&&) = default;
    AudioClip& operator=(AudioClip&&) = default;

    MIX_Audio* GetAudio() const { return m_Audio.get(); }

    const ResourceName& GetName() const { return m_Name; }

private:

    struct MIX_Audio_Deleter
    {
        void operator()(MIX_Audio* ptr) const;
    };

    using AudioPtr = std::unique_ptr<MIX_Audio, MIX_Audio_Deleter>;

    friend class AudioClipLoader;

    explicit AudioClip(const ResourceName& name,AudioPtr audio);

    ResourceName m_Name;

    AudioPtr m_Audio;
};
#include "AudioClip.h"

#include <SDL3_mixer/SDL_mixer.h>

void
AudioClip::MIX_Audio_Deleter::operator()(MIX_Audio* ptr) const
{
    if(ptr)
    {
        MIX_DestroyAudio(ptr);
    }
}

AudioClip::AudioClip(const ResourceName& name, AudioPtr audio)
    : m_Name(name),
      m_Audio(std::move(audio))
{
}
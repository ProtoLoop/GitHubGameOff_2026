#include "AudioClipLoader.h"

#include "AudioPlayer.h"
#include "ResourceDefs.h"

#include <memory>

#include <SDL3/SDL_iostream.h>
#include <SDL3_mixer/SDL_mixer.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <limits>
#include <span>
#endif

namespace
{
Result<MIX_Audio*>
LoadAudioFromMemory(MIX_Mixer& mixer, const FetchRequestPtr& rqstPtr)
{
    auto dataResult = rqstPtr->GetData();
    GG_CHECKB(dataResult, "Failed to get fetch data");

    SDL_IOStream* io = SDL_IOFromConstMem(dataResult->data(), dataResult->size());
    GG_CHECKB(io, "Failed to create SDL_IOStream from memory: {}", SDL_GetError());

    MIX_Audio* audio = MIX_LoadAudio_IO(&mixer,
        io,
        /*predecode=*/false,
        /*closeio=*/true);
    GG_CHECKB(audio, "Failed to load audio: {}", SDL_GetError());

    return audio;
}

} // namespace

Result<AudioClipLoader>
AudioClipLoader::Load(const AudioClipDef& audioClipDef,
    FileFetcher& fileFetcher,
    AudioPlayer& player)
{
    auto fetchResult = fileFetcher.Fetch(audioClipDef.FilePath);
    GG_CHECK(fetchResult, "Failed to fetch file: {}", audioClipDef.FilePath);

    return AudioClipLoader(audioClipDef.Name, std::move(*fetchResult), player);
}

bool
AudioClipLoader::IsPending() const
{
    return m_FetchRequest && m_FetchRequest->IsPending();
}

Result<>
AudioClipLoader::Update()
{
    if(m_FetchRequest && m_FetchRequest->IsPending())
    {
        //auto result = m_FetchRequest->Update();
        //GG_CHECK(result, "Failed to update fetch request");
    }
    return Result<>::Ok;
}

Result<AudioClip>
AudioClipLoader::Take()
{
    GG_CHECKB(!m_Taken, "Audio clip has already been taken");
    GG_CHECKB(!IsPending(), "Audio clip fetch request is still pending");

    m_Taken = true;

    auto audioResult = LoadAudioFromMemory(m_Player->GetMixer(), m_FetchRequest);
    GG_CHECKB(audioResult, "Failed to load audio: {}", m_Name);

    return AudioClip(m_Name, AudioClip::AudioPtr(*audioResult));
}
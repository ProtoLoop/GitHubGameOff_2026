#include "AudioPlayer.h"

#include "Defer.h"

#include <SDL3_mixer/SDL_mixer.h>

void
AudioPlayer::MIX_Mixer_Deleter::operator()(MIX_Mixer* ptr) const
{
    if(ptr)
    {
        MIX_DestroyMixer(ptr);
        MIX_Quit();
    }
}

void
AudioPlayer::MIX_Track_Deleter::operator()(MIX_Track* ptr) const
{
    if(ptr)
    {
        MIX_DestroyTrack(ptr);
    }
}

Result<AudioPlayer>
AudioPlayer::Create()
{
    GG_CHECK(MIX_Init(), "Error initializing mixer: {}", SDL_GetError());

    auto mixQuit = GG_DEFER
    {
        MIX_Quit();
    };
    
    MIX_Mixer* mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    GG_CHECK(mixer, "Error creating mixer: {}", SDL_GetError());

    MixerPtr mixerPtr(mixer);

    // Any failure from here will trigger mixer destruction and MIX_Quit
    mixQuit.cancel();

    BoundedVector<TrackInfo> tracks{ kMaxTracks };

    for(size_t i = 0; i < kMaxTracks; ++i)
    {
        MIX_Track* track = MIX_CreateTrack(mixer);
        GG_CHECK(track, "Error creating track: {}", SDL_GetError());

        tracks.emplace_back(TrackPtr(track), i);
    }

    return AudioPlayer(std::move(mixerPtr), std::move(tracks));
}

AudioPlayer::AudioPlayer(MixerPtr&& mixer, BoundedVector<TrackInfo>&& trackPool)
    : m_Mixer(std::move(mixer)),
      m_TrackPool(std::move(trackPool)),
      m_Playing(m_TrackPool.size())
{
}

MIX_Mixer&
AudioPlayer::GetMixer() const
{
    return *m_Mixer;
}

Result<AudioPlayer::TrackId>
AudioPlayer::Play(const AudioClip& clip, const PlayParams& params)
{
    GG_CHECKB(!m_TrackPool.empty(), "No available tracks to play");

    TrackInfo* trackInfo = &m_TrackPool.back();

    GG_CHECKB(MIX_SetTrackAudio(trackInfo->m_TrackPtr.get(), clip.GetAudio()),
        "Error setting track audio: {}",
        SDL_GetError());

    GG_CHECKB(MIX_SetTrackGain(trackInfo->m_TrackPtr.get(), params.Gain),
        "Error setting track gain: {}", SDL_GetError());

    GG_CHECKB(MIX_PlayTrack(trackInfo->m_TrackPtr.get(), 0),
        "Error playing track: {}",
        SDL_GetError());

    ++trackInfo->m_TrackId.m_Generation;

    trackInfo = &m_Playing.emplace_back(std::move(*trackInfo));

    m_TrackPool.pop_back();

    return trackInfo->m_TrackId;
}

Result<AudioPlayer::TrackId>
AudioPlayer::Loop(const AudioClip& clip, const PlayParams& params)
{
    GG_CHECKB(!m_TrackPool.empty(), "No available tracks to play");

    TrackInfo* trackInfo = &m_TrackPool.back();

    GG_CHECKB(MIX_SetTrackAudio(trackInfo->m_TrackPtr.get(), clip.GetAudio()),
        "Error setting track audio: {}",
        SDL_GetError());

    const SDL_PropertiesID options = SDL_CreateProperties();
    GG_CHECKB(options, "Error creating SDL properties: {}", SDL_GetError());

    auto cleanup = GG_DEFER
    {
        SDL_DestroyProperties(options);
    };

    GG_CHECKB(SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1),
        "Error setting loop property: {}", SDL_GetError());

    GG_CHECKB(MIX_SetTrackGain(trackInfo->m_TrackPtr.get(), params.Gain),
        "Error setting track gain: {}", SDL_GetError());

    GG_CHECKB(MIX_PlayTrack(trackInfo->m_TrackPtr.get(), options),
        "Error playing track: {}", SDL_GetError());

    ++trackInfo->m_TrackId.m_Generation;

    trackInfo = &m_Playing.emplace_back(std::move(*trackInfo));

    m_TrackPool.pop_back();

    return trackInfo->m_TrackId;
}

void
AudioPlayer::Stop(const TrackId trackId)
{
    for(const TrackInfo& trackInfo : m_Playing)
    {
        if(trackInfo.m_TrackId == trackId)
        {
            MIX_StopTrack(trackInfo.m_TrackPtr.get(), 0);
            break;
        }
    }
}

Result<>
AudioPlayer::Tick(const float /*deltaTime*/)
{
    for(size_t i = 0; i < m_Playing.size();)
    {
        TrackInfo& trackInfo = m_Playing[i];

        if(!MIX_TrackPlaying(trackInfo.m_TrackPtr.get())
            && !MIX_TrackPaused(trackInfo.m_TrackPtr.get()))
        {
            m_TrackPool.emplace_back(std::move(trackInfo));
            m_Playing[i] = std::move(m_Playing.back());
            m_Playing.pop_back();
        }
        else
        {
            ++i;
        }
    }

    return Result<>::Ok;
}
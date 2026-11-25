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

    InlineVector<TrackInfo, kMaxTracks> tracks;

    for(size_t i = 0; i < kMaxTracks; ++i)
    {
        MIX_Track* track = MIX_CreateTrack(mixer);
        GG_CHECK(track, "Error creating track: {}", SDL_GetError());

        tracks.emplace_back(TrackPtr(track), i);
    }

    auto impl = std::make_unique<AudioPlayer::Impl>();
    impl->m_Mixer = std::move(mixerPtr);
    impl->m_TrackPool = std::move(tracks);

    return AudioPlayer(std::move(impl));
}

AudioPlayer::AudioPlayer(std::unique_ptr<Impl>&& impl)
    : m_Impl(std::move(impl))
{
}

MIX_Mixer&
AudioPlayer::GetMixer() const
{
    return *m_Impl->m_Mixer;
}

Result<>
AudioPlayer::Play(const AudioClip& clip)
{
    GG_CHECK(!m_Impl->m_TrackPool.empty(), "No available tracks to play");

    TrackInfo& trackInfo = m_Impl->m_TrackPool.back();

    GG_CHECKB(MIX_SetTrackAudio(trackInfo.m_TrackPtr.get(), clip.GetAudio()),
        "Error setting track audio: {}",
        SDL_GetError());

    GG_CHECKB(MIX_PlayTrack(trackInfo.m_TrackPtr.get(), 0),
        "Error playing track: {}",
        SDL_GetError());

    m_Impl->m_Playing.emplace_back(std::move(trackInfo));

    m_Impl->m_TrackPool.pop_back();

    return Result<>::Ok;
}

Result<>
AudioPlayer::Tick(const float /*deltaTime*/)
{
    for(size_t i = 0; i < m_Impl->m_Playing.size();)
    {
        TrackInfo& trackInfo = m_Impl->m_Playing[i];

        if(!MIX_TrackPlaying(trackInfo.m_TrackPtr.get())
            && !MIX_TrackPaused(trackInfo.m_TrackPtr.get()))
        {
            m_Impl->m_TrackPool.emplace_back(std::move(trackInfo));
            m_Impl->m_Playing[i] = std::move(m_Impl->m_Playing.back());
            m_Impl->m_Playing.pop_back();
        }
        else
        {
            ++i;
        }
    }

    return Result<>::Ok;
}
#pragma once

#include "AudioClip.h"
#include "BoundedVector.h"
#include "Result.h"

#include <memory>

struct MIX_Mixer;
struct MIX_Track;

class AudioPlayer
{
public:

    static constexpr size_t kMaxTracks = 256;

    ~AudioPlayer() = default;
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = default;
    AudioPlayer& operator=(AudioPlayer&&) = default;

    static Result<AudioPlayer> Create();

    MIX_Mixer& GetMixer() const;

    Result<> Play(const AudioClip& clip);

    Result<> Tick(const float deltaTime);

private:

    struct MIX_Mixer_Deleter
    {
        void operator()(MIX_Mixer* ptr) const;
    };

    struct MIX_Track_Deleter
    {
        void operator()(MIX_Track* ptr) const;
    };

    using MixerPtr = std::unique_ptr<MIX_Mixer, MIX_Mixer_Deleter>;
    using TrackPtr = std::unique_ptr<MIX_Track, MIX_Track_Deleter>;

    struct TrackInfo
    {
        TrackInfo(TrackPtr&& trackPtr, const size_t index)
            : m_TrackPtr(std::move(trackPtr)),
              m_Index(index)
        {
        }

        TrackPtr m_TrackPtr;
        size_t m_Index;
    };

    AudioPlayer(MixerPtr&& mixer, BoundedVector<TrackInfo>&& trackPool);

    MixerPtr m_Mixer;
    BoundedVector<TrackInfo> m_TrackPool;
    BoundedVector<TrackInfo> m_Playing;
};
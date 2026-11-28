#pragma once

#include "AudioClip.h"
#include "BoundedVector.h"
#include "Result.h"

#include <limits>
#include <memory>

struct MIX_Mixer;
struct MIX_Track;

class AudioPlayer
{
public:
    static constexpr size_t kMaxTracks = 256;

    class TrackId
    {
    public:
        TrackId() = default;

        auto friend operator<=>(const TrackId& lhs, const TrackId& rhs) = default;

    private:
        friend class AudioPlayer;
        friend struct TrackInfo;

        explicit TrackId(const size_t trackIndex)
            : m_TrackIndex(trackIndex)
        {
        }

        size_t m_TrackIndex{std::numeric_limits<size_t>::max()};
        uint32_t m_Generation{ 0 };
    };

    struct PlayParams
    {
        float Gain{ 1 };
    };

    ~AudioPlayer() = default;
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = default;
    AudioPlayer& operator=(AudioPlayer&&) = default;

    static Result<AudioPlayer> Create();

    MIX_Mixer& GetMixer() const;

    /// Plays the audio clip once on an available track. Returns an error if no tracks are
    /// available or if playback fails.
    Result<TrackId> Play(const AudioClip& clip) { return Play(clip, PlayParams{}); }

    /// Plays the audio clip once on an available track. Returns an error if no tracks are
    /// available or if playback fails.
    Result<TrackId> Play(const AudioClip& clip, const PlayParams& params);

    /// Loops the audio clip on an available track indefinitely. Returns an error if no tracks are
    /// available or if playback fails.
    Result<TrackId> Loop(const AudioClip& clip) { return Loop(clip, PlayParams{}); }

    /// Loops the audio clip on an available track indefinitely. Returns an error if no tracks are
    /// available or if playback fails.
    Result<TrackId> Loop(const AudioClip& clip, const PlayParams& params);

    /// Stops playback of the specified track. Does nothing if the track is not currently playing.
    void Stop(const TrackId trackId);

    /// Must be called periodically to update the state of playing tracks and free up completed
    /// tracks.
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
              m_TrackId{ index }
        {
        }

        TrackPtr m_TrackPtr;
        TrackId m_TrackId;
    };

    AudioPlayer(MixerPtr&& mixer, BoundedVector<TrackInfo>&& trackPool);

    MixerPtr m_Mixer;
    BoundedVector<TrackInfo> m_TrackPool;
    BoundedVector<TrackInfo> m_Playing;
};
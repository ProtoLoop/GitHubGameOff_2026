#pragma once

#include "Timer.h"
#include "EntityId.h"
#include "Result.h"

class Game;
class AudioClip;

class AmbientAudio
{
public:
    AmbientAudio() = delete;
    ~AmbientAudio() = default;
    AmbientAudio(const AmbientAudio&) = delete;
    AmbientAudio& operator=(const AmbientAudio&) = delete;
    AmbientAudio(AmbientAudio&&) = default;
    AmbientAudio& operator=(AmbientAudio&&) = default;

    static Result<AmbientAudio> Create(Game& game);

    Result<> Tick(const float deltaTime, Game& game);

    EntityId GetEntityId() const { return m_EntityId; }

private:

    struct AudioClips
    {
        const AudioClip* Crickets;
        const AudioClip* Crow[2];
        const AudioClip* Owl[3];
        const AudioClip* Wolf[4];
        const AudioClip* Peacock[2];
        const AudioClip* Dog[1];
    };

    explicit AmbientAudio(const AudioClips& audioClips);

    EntityId m_EntityId{ EntityId::NextId() };

    AudioClips m_AudioClips;

    CountdownTimer m_CrowTimer;
    CountdownTimer m_OwlTimer;
    CountdownTimer m_WolfTimer;
    CountdownTimer m_PeacockTimer;
    CountdownTimer m_DogTimer;
};
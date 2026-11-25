#pragma once

#include "Actors/Caretaker.h"
#include "Actors/LostSoul.h"
#include "AudioPlayer.h"
#include "Level.h"
#include "Prop.h"
#include "Random.h"

#include <memory>

class Input;
class Renderer;
class AudioPlayer;

class Game
{
public:
    static constexpr float kFrameDuration = 1.0f / 60.0f;
    /// The maximum allowed delta time for a single tick to prevent large time steps.
    static constexpr float kMaxDeltaTime = 0.25f;

    Game() = delete;
    ~Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    Game(Input& input,
        std::unique_ptr<Level>&& level,
        std::unique_ptr<Renderer>&& renderer,
        AudioPlayer&& audioPlayer);

    Result<> Begin();

    Input& GetInput() { return *m_Input; }
    const Input& GetInput() const { return *m_Input; }

    Renderer& GetRenderer() { return *m_Renderer; }
    const Renderer& GetRenderer() const { return *m_Renderer; }

    Level& GetLevel() { return *m_Level; }
    const Level& GetLevel() const { return *m_Level; }

    AudioPlayer& GetAudioPlayer() { return m_AudioPlayer; }
    const AudioPlayer& GetAudioPlayer() const { return m_AudioPlayer; }

    Random& GetEntityRng() { return m_EntityRng; }

    Result<const LostSoul&> SpawnLostSoul(const Transform2D& transform);

    Result<> QueueDespawn(const LostSoul& lostSoul);

    Result<> Tick(const float deltaTime);

    bool IsRunning() const { return m_Stage == Stage::Running; }

private:
    static constexpr size_t kMaxLostSouls = 10;

    enum class Stage
    {
        None,
        Running,
    };

    Result<> TickEntities(float deltaTime);

    Result<> RenderSprites();

    void QueueFrameRect(const SpriteFrame& spriteFrame, const Transform2D& transform);

    void QueueCollisionRects(const SpriteFrame& spriteFrame, const Transform2D& transform);

    void CheckPropCollisions();

    void CheckLostSoulCollisions();

    void CommitDespawnQueues();

    Stage m_Stage{Stage::None};

    Input* m_Input;
    std::unique_ptr<Level> m_Level;
    std::unique_ptr<Renderer> m_Renderer;
    AudioPlayer m_AudioPlayer;
    
    std::optional<Caretaker> m_Caretaker;

    InlineVector<LostSoul, kMaxLostSouls> m_LostSouls;

    InlineVector<EntityId, kMaxLostSouls> m_DespawnQueue;

    float m_TimeAccum{ 0.0f };

    static constexpr uint64_t EntityRngSeed = 42;
    static constexpr uint64_t EntityRngStream = 54;
    Random m_EntityRng{EntityRngSeed, EntityRngStream};
};
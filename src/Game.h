#pragma once

#include "Actors/Caretaker.h"
#include "Actors/LostSoul.h"
#include "Level.h"
#include "Prop.h"
#include "Random.h"

#include <memory>

class Input;

class Game
{
public:
    static constexpr float kSecondsPerTick = 1.0f / 60.0f;
    static constexpr float kMaxDeltaSec = 0.25f;

    Game() = delete;
    ~Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;
    
    Game(Input& input, std::unique_ptr<Level>&& level, std::unique_ptr<Renderer>&& renderer);

    Result<> Begin();

    Input& GetInput() { return *m_Input; }
    const Input& GetInput() const { return *m_Input; }

    Renderer& GetRenderer() { return *m_Renderer; }
    const Renderer& GetRenderer() const { return *m_Renderer; }

    Level& GetLevel() { return *m_Level; }
    const Level& GetLevel() const { return *m_Level; }

    Random& GetEntityRng() { return m_EntityRng; }

    Result<> Tick(const float deltaSec);

    bool IsRunning() const { return m_Stage == Stage::Running; }

private:
    static constexpr size_t kMaxLostSouls = 10;
    static constexpr size_t kMaxProps = 10;

    enum class Stage
    {
        None,
        Running,
    };

    Result<> TickEntities(float deltaSec);

    Result<> RenderSprites(Renderer& renderer);

    Stage m_Stage{Stage::None};

    Input* m_Input;
    std::unique_ptr<Level> m_Level;
    std::unique_ptr<Renderer> m_Renderer;

    std::optional<Caretaker> m_Caretaker;

    InlineVector<LostSoul, kMaxLostSouls> m_LostSouls;

    InlineVector<Prop, kMaxProps> m_Props;

    float m_TimeAccum{ 0.0f };

    static constexpr uint64_t EntityRngSeed = 42;
    static constexpr uint64_t EntityRngStream = 54;
    Random m_EntityRng{EntityRngSeed, EntityRngStream};

    static constexpr uint64_t PropRngSeed = 54;
    static constexpr uint64_t PropRngStream = 42;
    Random m_PropRng{PropRngSeed, PropRngStream};
};
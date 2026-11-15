#pragma once

#include "Entities.h"
#include "Level.h"

class Input;

class Game
{
public:
    Game() = delete;
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;
    
    Game(Input& input, Level&& level, std::unique_ptr<Renderer>&& renderer);

    Result<> Begin();

    Renderer& GetRenderer() { return *m_Renderer; }
    const Renderer& GetRenderer() const { return *m_Renderer; }

    Level& GetLevel() { return m_Level; }
    const Level& GetLevel() const { return m_Level; }

    Caretaker& GetCaretaker()
    {
        GG_ASSERT(IsRunning(), "Game is not running");
        return *m_Caretaker;
    }
    
    const Caretaker& GetCaretaker() const
    {
        GG_ASSERT(IsRunning(), "Game is not running");
        return *m_Caretaker;
    }

    Result<> Tick(float deltaSec);

    bool IsRunning() const { return m_Stage == Stage::Running; }

private:
    enum class Stage
    {
        None,
        Running,
    };

    Stage m_Stage{Stage::None};

    Input* m_Input;
    Level m_Level;
    std::unique_ptr<Renderer> m_Renderer;

    std::optional<Caretaker> m_Caretaker;
};
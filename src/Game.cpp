#include "Game.h"

#include "Renderer.h"

Game::Game(Input& input, Level&& level, std::unique_ptr<Renderer>&& renderer)
    : m_Input(&input),
      m_Level(std::move(level)),
      m_Renderer(std::move(renderer))
{
}

Game::~Game()
{
    if(m_Caretaker)
    {
        m_Caretaker->Destroy(m_Level);
    }
}

Result<>
Game::Begin()
{
    GG_CHECKB(m_Stage == Stage::None, "Game has already begun");

    GG_TRYB(caretaker, Caretaker::Create(m_Level), "Failed to create caretaker");

    m_Caretaker = std::move(caretaker);
    m_Stage = Stage::Running;
    return Result<>::Ok;
}

Result<>
Game::Tick(float deltaSec)
{
    GG_CHECKB(m_Stage == Stage::Running, "Game is not running");

    GG_CHECK(m_Caretaker->Tick(deltaSec, *m_Input, m_Level));
    m_Level.Tick(deltaSec);

    m_Renderer->SetView(m_Caretaker->GetPos().x, m_Caretaker->GetPos().y);

    return Result<>::Ok;
}
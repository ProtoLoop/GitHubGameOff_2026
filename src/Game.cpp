#include "Game.h"

#include "Renderer.h"

Game::Game(Input& input, std::unique_ptr<Level>&& level, std::unique_ptr<Renderer>&& renderer)
    : m_Input(&input),
      m_Level(std::move(level)),
      m_Renderer(std::move(renderer))
{
}

Result<>
Game::Begin()
{
    GG_CHECKB(m_Stage == Stage::None, "Game has already begun");

    GG_TRYB(caretaker, Caretaker::Create(*this), "Failed to create caretaker");

    m_Caretaker = std::move(caretaker);

    for(size_t i = 0; i < kMaxLostSouls; ++i)
    {
        GG_TRYB(lostSoul, LostSoul::Create(*this), "Failed to create lost soul");
        // NOLINTNEXTLINE
        lostSoul.MoveTo(m_EntityRng.NextInRange(-14.0f, 14.0f), m_EntityRng.NextInRange(-14.0f, 14.0f));
        m_LostSouls.push_back(std::move(lostSoul));
    }

    m_Stage = Stage::Running;
    return Result<>::Ok;
}

Result<>
Game::Tick(const float deltaSec)
{
    GG_CHECKB(m_Stage == Stage::Running, "Game is not running");

    if(deltaSec > kMaxDeltaSec)
    {
        m_TimeAccum += kMaxDeltaSec;
    }
    else
    {
        m_TimeAccum += deltaSec;
    }

    while(m_TimeAccum >= kSecondsPerTick)
    {
        m_TimeAccum -= kSecondsPerTick;
        GG_CHECK(TickEntities(kSecondsPerTick));
    }

    CheckCollisions();

    m_Renderer->SetView(m_Caretaker->GetPos().x, m_Caretaker->GetPos().y);

    GG_CHECK(RenderSprites(*m_Renderer));

    return Result<>::Ok;
}

// private:

Result<>
Game::TickEntities(float deltaSec)
{
    GG_CHECK(m_Caretaker->Tick(deltaSec, *this));
    for(auto& lostSoul : m_LostSouls)
    {
        GG_CHECK(lostSoul.Tick(deltaSec, *this));
    }
    return Result<>::Ok;
}

Result<>
Game::RenderSprites(Renderer& renderer)
{
    GG_CHECK(m_Caretaker->Render(renderer));

    for(const LostSoul& lostSoul : m_LostSouls)
    {
        GG_CHECK(lostSoul.Render(renderer));
    }

    for(const Prop& prop : m_Level->GetProps())
    {
        GG_CHECK(prop.Render(renderer));
    }

    return Result<>::Ok;
}

void
Game::CheckCollisions()
{
    const float cellPixPerSide = static_cast<float>(m_Renderer->GetCellPixelsPerSide());

    const glm::vec2 pixelScale = glm::vec2(1, -1) * cellPixPerSide;

    const glm::mat2x2& proj = m_Renderer->GetProjectionMatrix();

    const Sprite& ctSprite = m_Caretaker->GetSprite();
    const Rectf ctRect = ctSprite.GetRect();
    Rectf ctBounds = Rectf(ctRect.Size()) - (ctRect.Size() * ctSprite.GetPivot());
    ctBounds *= ctSprite.GetScale();
    ctBounds += (proj * m_Caretaker->GetPos() * pixelScale); // Offset by projected position

    for(const Prop& prop : m_Level->GetProps())
    {
        const Sprite& propSprite = prop.GetSprite();
        const Rectf propRect{ propSprite.GetRect().Size() };
        Rectf propBounds = Rectf(propRect.Size()) - (propRect.Size() * propSprite.GetPivot());
        propBounds *= propSprite.GetScale();
        propBounds += (proj * prop.GetPos() * pixelScale); // Offset by projected position

        if(ctBounds.Intersects(propBounds))
        {
            //GG_INFO("Caretaker collided with prop");
        }
    }
}
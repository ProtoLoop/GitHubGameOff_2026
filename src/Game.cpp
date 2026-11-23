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
        const float x = m_EntityRng.NextInRange(-14.0f, 14.0f);
        const float y = m_EntityRng.NextInRange(-14.0f, 14.0f);
        lostSoul.MoveTo(x, y);
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

    GG_CHECK(RenderSprites());

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
Game::RenderSprites()
{
    Renderer& renderer = *m_Renderer;

    GG_CHECK(m_Caretaker->Render(renderer));

    QueueFrameRect(m_Caretaker->GetSpriteFrame(), m_Caretaker->GetTransform());
    QueueCollisionRects(m_Caretaker->GetSpriteFrame(), m_Caretaker->GetTransform());

    for(const LostSoul& lostSoul : m_LostSouls)
    {
        GG_CHECK(lostSoul.Render(renderer));

        const SpriteFrame& spriteFrame = lostSoul.GetSpriteFrame();

        QueueFrameRect(spriteFrame, lostSoul.GetTransform());
        QueueCollisionRects(spriteFrame, lostSoul.GetTransform());
    }

    for(const Prop& prop : m_Level->GetProps())
    {
        GG_CHECK(prop.Render(renderer));
        const SpriteFrame& spriteFrame = prop.GetSpriteFrame();

        QueueFrameRect(spriteFrame, prop.GetTransform());
        QueueCollisionRects(spriteFrame, prop.GetTransform());
    }

    return Result<>::Ok;
}

void
Game::QueueFrameRect(const SpriteFrame& spriteFrame, const Transform2D& transform)
{
    static constexpr glm::vec4 kYellow{ 1, 1, 0, 1 };

    Renderer& renderer = *m_Renderer;

    Rectf drawRect{ spriteFrame.GetSize() };
    drawRect -= drawRect.Size() * spriteFrame.GetPivot();
    drawRect *= spriteFrame.GetScale();

    if(!renderer.QueueRectangle(drawRect, transform, kYellow))
    {
        GG_ERROR("Failed to queue rectangle for prop");
    }
}

void
Game::QueueCollisionRects(const SpriteFrame& spriteFrame, const Transform2D& transform)
{
    static constexpr glm::vec4 kMagenta{ 1, 0, 1, 1 };

    Renderer& renderer = *m_Renderer;

    const glm::vec2 rectSize = spriteFrame.GetSize();

    for(const Rectf colRect : spriteFrame.GetCollisionRects())
    {
        Rectf drawRect = colRect - spriteFrame.GetPivot();

        drawRect.x *= rectSize.x;
        drawRect.y *= rectSize.y;
        drawRect.w *= rectSize.x;
        drawRect.h *= rectSize.y;
        drawRect *= spriteFrame.GetScale();

        if(!renderer.QueueRectangle(drawRect, transform, kMagenta))
        {
            GG_ERROR("Failed to queue rectangle for prop collision rect");
            break;
        }
    }
}

void
Game::CheckCollisions()
{
    const float cellPixPerSide = static_cast<float>(m_Renderer->GetCellPixelsPerSide());
    const glm::vec2 pixelScale = glm::vec2(1, -1) * cellPixPerSide;

    const glm::mat2x2& proj = m_Renderer->GetProjectionMatrix();

    const SpriteFrame& ctSpriteFrame = m_Caretaker->GetSpriteFrame();

    float collisionDist = std::numeric_limits<float>::max();
    glm::vec2 collisionNormal;
    bool hasCollision = false;

    const glm::vec2& ctPos = m_Caretaker->GetPos();

    const Rectf ctRect = ctSpriteFrame.GetScreenRect(ctSpriteFrame.GetCollisionBounds(),
        ctPos,
        proj,
        cellPixPerSide);

    for(const Prop& prop : m_Level->GetProps())
    {
        const glm::vec2& propPos = prop.GetPos();
        const SpriteFrame& propSpriteFrame = prop.GetSpriteFrame();
        const Rectf propRect = propSpriteFrame.GetScreenRect(propSpriteFrame.GetCollisionBounds(),
            propPos,
            proj,
            cellPixPerSide);

        if(!ctRect.Intersects(propRect))
        {
            continue;
        }

        GG_INFO("****");

        const glm::vec2 ctSrcSize = ctSpriteFrame.GetSize();
        const glm::vec2 propSrcSize = propSpriteFrame.GetSize();

        for(uint32_t i = 0; i < ctSpriteFrame.GetCollisionRects().size(); ++i)
        {
            const Rectf ctBounds =
                ctSpriteFrame.GetCollisionScreenRect(i, ctPos, proj, cellPixPerSide);

            for(uint32_t j = 0; j < propSpriteFrame.GetCollisionRects().size(); ++j)
            {
                const Rectf propBounds =
                    propSpriteFrame.GetCollisionScreenRect(j, propPos, proj, cellPixPerSide);

                if(ctBounds.Intersects(propBounds))
                {
                    const float distR = propBounds.Right() - ctBounds.Left();
                    const float distL = ctBounds.Right() - propBounds.Left();
                    const float distT = ctBounds.Bottom() - propBounds.Top();
                    const float distB = propBounds.Bottom() - ctBounds.Top();

                    const float minDist = std::min({ distR, distL, distT, distB });
                    if(minDist >= collisionDist)
                    {
                        continue; // Skip if this is not the closest collision
                    }

                    if(minDist <= 0)
                    {
                        continue; // Skip if the collision distance is zero
                    }

                    collisionDist = minDist;
                    if(minDist == distR)
                    {
                        // Right collision
                        collisionNormal = glm::vec2(1, 0);
                    }
                    else if(minDist == distL)
                    {
                        // Left collision
                        collisionNormal = glm::vec2(-1, 0);
                    }
                    else if(minDist == distT)
                    {
                        // Top collision
                        collisionNormal = glm::vec2(0, -1);
                    }
                    else if(minDist == distB)
                    {
                        // Bottom collision
                        collisionNormal = glm::vec2(0, 1);
                    }

                    // Unproject the collision normal.
                    collisionNormal = glm::inverse(proj) * collisionNormal;

                    hasCollision = true;
                }
            }
        }
    }

    if(hasCollision)
    {
        m_Caretaker->MoveBy(collisionNormal * collisionDist / pixelScale);
    }
}
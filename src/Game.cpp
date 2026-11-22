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

    Rectf rect{ spriteFrame.GetSize() };
    rect -= rect.Size() * spriteFrame.GetPivot();

    if(!renderer.QueueRectangle(rect, transform, spriteFrame.GetScale(), kYellow))
    {
        GG_ERROR("Failed to queue rectangle for prop");
    }
}

void
Game::QueueCollisionRects(const SpriteFrame& spriteFrame, const Transform2D& transform)
{
    static constexpr glm::vec4 kMagenta{ 1, 0, 1, 1 };

    Renderer& renderer = *m_Renderer;

    Rectf rect{ spriteFrame.GetSize() };
    rect -= rect.Size() * spriteFrame.GetPivot();

    const std::span<const Rectf> colRects = spriteFrame.GetCollisionRects();

    const glm::vec2 rectSize = spriteFrame.GetSize();

    for(const Rectf colRect : colRects)
    {
        Rectf drawRect //
            {
                colRect.x * rect.Size().x,
                colRect.y * rect.Size().y,
                colRect.w * rect.Size().x,
                colRect.h * rect.Size().y,
            };

        drawRect -= rectSize * spriteFrame.GetPivot();
        if(!renderer.QueueRectangle(drawRect, transform, spriteFrame.GetScale(), kMagenta))
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
    const glm::vec2 ctRectScale = ctSpriteFrame.GetSize();

    float collisionDist = std::numeric_limits<float>::max();
    glm::vec2 collisionNormal;
    bool hasCollision = false;

    for(Rectf ctBounds : ctSpriteFrame.GetCollisionRects())
    {
        ctBounds -= ctSpriteFrame.GetPivot();
        ctBounds.x *= ctRectScale.x;
        ctBounds.y *= ctRectScale.y;
        ctBounds.w *= ctRectScale.x;
        ctBounds.h *= ctRectScale.y;
        ctBounds *= ctSpriteFrame.GetScale();
        ctBounds += (proj * m_Caretaker->GetPos() * pixelScale); // Offset by projected position

        for(const Prop& prop : m_Level->GetProps())
        {
            const SpriteFrame& propSpriteFrame = prop.GetSpriteFrame();
            const glm::vec2 propRectScale = propSpriteFrame.GetSize();

            for(Rectf propBounds : propSpriteFrame.GetCollisionRects())
            {
                propBounds -= propSpriteFrame.GetPivot();
                propBounds.x *= propRectScale.x;
                propBounds.y *= propRectScale.y;
                propBounds.w *= propRectScale.x;
                propBounds.h *= propRectScale.y;
                propBounds *= propSpriteFrame.GetScale();
                propBounds += (proj * prop.GetPos() * pixelScale); // Offset by projected position

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
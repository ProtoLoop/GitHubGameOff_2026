#include "Game.h"

#include "AudioPlayer.h"
#include "Renderer.h"
#include "ResourceDefs.h"

namespace
{
constexpr bool kDrawRectangles = false;
} // namespace

Game::Game(Input& input,
    std::unique_ptr<Level>&& level,
    std::unique_ptr<Renderer>&& renderer,
    AudioPlayer&& audioPlayer)
    : m_Input(&input),
      m_Level(std::move(level)),
      m_Renderer(std::move(renderer)),
      m_AudioPlayer(std::move(audioPlayer))
{
}

Result<Game>
Game::Create(Input& input,
    const LevelDef& levelDef,
    std::unique_ptr<Level>&& level,
    std::unique_ptr<Renderer>&& renderer,
    AudioPlayer&& audioPlayer)
{
    Game game(input, std::move(level), std::move(renderer), std::move(audioPlayer));

    for(const PropInstanceDef& propDef : levelDef.PropInstDefs)
    {
        const auto propTypeDefResult = levelDef.GetPropTypeDef(propDef.TypeName);
        GG_CHECK(propTypeDefResult, "Failed to get prop type definition for {}", propDef.TypeName);

        Transform2D transform = propDef.Transform;

        // Default scale is set on the type definition.
        // It can be overridden by the instance definition.
        transform.Scale = propDef.OptScale.value_or(propTypeDefResult->Scale);

        GG_CHECK(game.SpawnProp(propTypeDefResult->SpriteName, transform),
            "Failed to spawn prop: {}",
            propTypeDefResult->SpriteName);
    }

    for(const ActorInstanceDef& actorDef : levelDef.ActorInstDefs)
    {
        const auto actorTypeDefResult = levelDef.GetActorTypeDef(actorDef.TypeName);
        GG_CHECK(actorTypeDefResult, "Failed to get actor type definition for {}", actorDef.TypeName);

        if(GG_VERIFYB(actorTypeDefResult->ClassName == "Tombstone",
               "Unknown actor class: {}",
               actorTypeDefResult->ClassName))
        {
            Transform2D transform = actorDef.Transform;

            // Default scale is set on the type definition.
            // It can be overridden by the instance definition.
            transform.Scale = actorDef.OptScale.value_or(actorTypeDefResult->Scale);

            GG_CHECK(game.SpawnTombstone(actorTypeDefResult->SpriteName, transform),
                "Failed to spawn tombstone: {}",
                actorTypeDefResult->SpriteName);
        }
    }
    return std::move(game);
}

Result<>
Game::Begin()
{
    GG_CHECKB(m_Stage == Stage::None, "Game has already begun");

    GG_TRYB(caretaker, Caretaker::Create(*this), "Failed to create caretaker");

    m_Caretaker = std::move(caretaker);

    m_Stage = Stage::Running;
    return Result<>::Ok;
}

Result<>
Game::SpawnProp(const std::string_view spriteName, const Transform2D& transform)
{
    GG_CHECKB(m_Props.size() < m_Props.capacity(), "Maximum number of props reached");

    auto spriteResult = m_Level->GetSprite(spriteName);
    GG_CHECKB(spriteResult, "Failed to get sprite: {}", spriteName);

    auto spriteInstanceResult = spriteResult->CreateInstance();
    GG_CHECKB(spriteInstanceResult, "Failed to create sprite instance for {}", spriteName);

    m_Props.emplace_back(*spriteInstanceResult, transform);

    return Result<>::Ok;
}

Result<EntityId>
Game::SpawnTombstone(const std::string_view spriteName, const Transform2D& transform)
{
    GG_CHECKB(m_Tombstones.size() < m_Tombstones.capacity(), "Maximum number of tombstones reached");

    GG_TRYB(tombstone,
        Tombstone::Create(*this, spriteName),
        "Failed to create tombstone: {}",
        spriteName);
    tombstone.SetTransform(transform);
    return m_Tombstones.emplace_back(std::move(tombstone)).GetEntityId();
}

Result<EntityId>
Game::SpawnLostSoul(const Transform2D& transform)
{
    GG_CHECKB(m_LostSouls.size() < m_LostSouls.capacity(), "Maximum number of lost souls reached");

    GG_TRYB(lostSoul, LostSoul::Create(*this), "Failed to create lost soul");
    lostSoul.SetTransform(transform);
    return m_LostSouls.emplace_back(std::move(lostSoul)).GetEntityId();
}

Result<>
Game::QueueDespawn(const LostSoul& lostSoul)
{
    bool inQueue = false;

    for(const EntityId& id : m_DespawnQueue)
    {
        if(id == lostSoul.GetEntityId())
        {
            inQueue = true;
            break;
        }
    }

    if(inQueue)
    {
        return Result<>::Ok;
    }

    bool isValid = false;

    // Not in the queue - make sure it's valid before adding to the despawn queue
    for(const LostSoul& ls : m_LostSouls)
    {
        if(ls.GetEntityId() == lostSoul.GetEntityId())
        {
            isValid = true;
            break;
        }
    }

    GG_CHECKB(isValid, "Lost soul not found");

    if(isValid)
    {
        m_DespawnQueue.push_back(lostSoul.GetEntityId());
    }

    return Result<>::Ok;
}

Result<>
Game::Tick(const float deltaTime)
{
    GG_CHECKB(m_Stage == Stage::Running, "Game is not running");

    if(deltaTime > kMaxDeltaTime)
    {
        m_TimeAccum += kMaxDeltaTime;
    }
    else
    {
        m_TimeAccum += deltaTime;
    }

    while(m_TimeAccum >= kFrameDuration)
    {
        m_TimeAccum -= kFrameDuration;
        GG_CHECK(TickEntities(kFrameDuration));
    }

    CheckPropCollisions();
    CheckLostSoulCollisions();

    m_Renderer->SetView(m_Caretaker->GetPos().x, m_Caretaker->GetPos().y);

    GG_CHECK(RenderSprites());

    GG_CHECK(m_AudioPlayer.Tick(deltaTime));

    CommitDespawnQueues();

    return Result<>::Ok;
}

// private:

Result<>
Game::TickEntities(float deltaTime)
{
    GG_CHECK(m_Caretaker->Tick(deltaTime, *this));

    for(Tombstone& tombstone : m_Tombstones)
    {
        GG_CHECK(tombstone.Tick(deltaTime, *this));
    }

    for(LostSoul& lostSoul : m_LostSouls)
    {
        GG_CHECK(lostSoul.Tick(deltaTime, *this));
    }
    return Result<>::Ok;
}

Result<>
Game::RenderSprites()
{
    Renderer& renderer = *m_Renderer;

    GG_CHECK(m_Caretaker->Render(renderer));

    if(kDrawRectangles)
    {
        QueueFrameRect(m_Caretaker->GetSpriteFrame(), m_Caretaker->GetTransform());
        QueueCollisionRects(m_Caretaker->GetSpriteFrame(), m_Caretaker->GetTransform());
    }

    for(const LostSoul& lostSoul : m_LostSouls)
    {
        GG_CHECK(lostSoul.Render(renderer));

        const SpriteFrame& spriteFrame = lostSoul.GetSpriteFrame();

        if(kDrawRectangles)
        {
            QueueFrameRect(spriteFrame, lostSoul.GetTransform());
            QueueCollisionRects(spriteFrame, lostSoul.GetTransform());
        }
    }

    for(const Tombstone& tombstone : m_Tombstones)
    {
        GG_CHECK(tombstone.Render(renderer));

        const SpriteFrame& spriteFrame = tombstone.GetSpriteFrame();

        if(kDrawRectangles)
        {
            QueueFrameRect(spriteFrame, tombstone.GetTransform());
            QueueCollisionRects(spriteFrame, tombstone.GetTransform());
        }
    }

    for(const Prop& prop : m_Props)
    {
        GG_CHECK(prop.Render(renderer));
        const SpriteFrame& spriteFrame = prop.GetSpriteFrame();

        if(kDrawRectangles)
        {
            QueueFrameRect(spriteFrame, prop.GetTransform());
            QueueCollisionRects(spriteFrame, prop.GetTransform());
        }
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
    drawRect *= transform.Scale;

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
        Rectf drawRect = colRect - (spriteFrame.GetPivot() * rectSize);

        drawRect *= transform.Scale;

        if(!renderer.QueueRectangle(drawRect, transform, kMagenta))
        {
            GG_ERROR("Failed to queue rectangle for prop collision rect");
            break;
        }
    }
}

void
Game::CheckPropCollisions()
{
    const float cellPixPerSide = static_cast<float>(m_Renderer->GetCellPixelsPerSide());
    const glm::vec2 pixelScale = glm::vec2(1, -1) * cellPixPerSide;

    const glm::mat2x2& proj = m_Renderer->GetProjectionMatrix();

    const SpriteFrame& ctSpriteFrame = m_Caretaker->GetSpriteFrame();

    float collisionDist = std::numeric_limits<float>::max();
    glm::vec2 collisionNormal;
    // Prop that was hit
    const Prop* propHit = nullptr;

    const glm::vec2& ctPos = m_Caretaker->GetPos();
    const float ctScale = m_Caretaker->GetScale();

    const Rectf ctRect = ctSpriteFrame.GetScreenRect(ctSpriteFrame.GetCollisionBounds(),
        ctPos,
        ctScale,
        proj,
        cellPixPerSide);

    const std::span<const Rectf> ctCollisionRects = ctSpriteFrame.GetCollisionRects();

    for(const Prop& prop : m_Props)
    {
        const glm::vec2& propPos = prop.GetPos();
        const float propScale = prop.GetTransform().Scale;
        const SpriteFrame& propSpriteFrame = prop.GetSpriteFrame();
        const Rectf propRect = propSpriteFrame.GetScreenRect(propSpriteFrame.GetCollisionBounds(),
            propPos,
            propScale,
            proj,
            cellPixPerSide);

        // FIXME(KB) - not needed if bot colliders only have one collision rect
        if(!ctRect.Intersects(propRect))
        {
            continue;
        }

        for(const Rectf& ctColRect : ctCollisionRects)
        {
            const Rectf ctBounds =
                ctSpriteFrame.GetScreenRect(ctColRect, ctPos, ctScale, proj, cellPixPerSide);

            const std::span<const Rectf> propCollisionRects = propSpriteFrame.GetCollisionRects();

            for(const Rectf& propColRect : propCollisionRects)
            {
                const Rectf propBounds = propSpriteFrame.GetScreenRect(propColRect,
                    propPos,
                    propScale,
                    proj,
                    cellPixPerSide);

                if(ctBounds.Intersects(propBounds))
                {
                    const float distR = propBounds.Right() - ctBounds.Left();
                    const float distL = ctBounds.Right() - propBounds.Left();
                    const float distT = ctBounds.Bottom() - propBounds.Top();
                    const float distB = propBounds.Bottom() - ctBounds.Top();

                    const float minDist = std::min({ distR, distL, distT, distB });
                    if(minDist <= 0 || minDist >= collisionDist)
                    {
                        // Skip if this is not the closest collision
                        // or if the collision distance is zero
                        continue;
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

                    propHit = &prop;
                }
            }
        }
    }

    if(propHit)
    {
        m_Caretaker->MoveBy(collisionNormal * collisionDist / pixelScale);
        m_Caretaker->OnPropHit(*this, *propHit);
    }
}

void
Game::CheckLostSoulCollisions()
{
    const float cellPixPerSide = static_cast<float>(m_Renderer->GetCellPixelsPerSide());

    const glm::mat2x2& proj = m_Renderer->GetProjectionMatrix();

    const SpriteFrame& ctSpriteFrame = m_Caretaker->GetSpriteFrame();

    float collisionDist = std::numeric_limits<float>::max();
    // Prop that was hit
    const LostSoul* lostSoulHit = nullptr;

    const glm::vec2& ctPos = m_Caretaker->GetPos();
    const float ctScale = m_Caretaker->GetScale();

    const Rectf ctRect = ctSpriteFrame.GetScreenRect(ctSpriteFrame.GetCollisionBounds(),
        ctPos,
        ctScale,
        proj,
        cellPixPerSide);

    const std::span<const Rectf> ctCollisionRects = ctSpriteFrame.GetCollisionRects();

    for(const LostSoul& lostSoul : m_LostSouls)
    {
        const glm::vec2& lostSoulPos = lostSoul.GetPos();
        const float lostSoulScale = lostSoul.GetScale();
        const SpriteFrame& lostSoulSpriteFrame = lostSoul.GetSpriteFrame();
        const Rectf lostSoulRect =
            lostSoulSpriteFrame.GetScreenRect(lostSoulSpriteFrame.GetCollisionBounds(),
                lostSoulPos,
                lostSoulScale,
                proj,
                cellPixPerSide);

        // FIXME(KB) - not needed if bot colliders only have one collision rect
        if(!ctRect.Intersects(lostSoulRect))
        {
            continue;
        }

        for(const Rectf& ctColRect : ctCollisionRects)
        {
            const Rectf ctBounds =
                ctSpriteFrame.GetScreenRect(ctColRect, ctPos, ctScale, proj, cellPixPerSide);

            const std::span<const Rectf> lostSoulCollisionRects =
                lostSoulSpriteFrame.GetCollisionRects();

            for(const Rectf& lostSoulColRect : lostSoulCollisionRects)
            {
                const Rectf lostSoulBounds = lostSoulSpriteFrame.GetScreenRect(lostSoulColRect,
                    lostSoulPos,
                    lostSoulScale,
                    proj,
                    cellPixPerSide);

                if(ctBounds.Intersects(lostSoulBounds))
                {
                    const float distR = lostSoulBounds.Right() - ctBounds.Left();
                    const float distL = ctBounds.Right() - lostSoulBounds.Left();
                    const float distT = ctBounds.Bottom() - lostSoulBounds.Top();
                    const float distB = lostSoulBounds.Bottom() - ctBounds.Top();

                    const float minDist = std::min({ distR, distL, distT, distB });
                    if(minDist <= 0 || minDist >= collisionDist)
                    {
                        // Skip if this is not the closest collision
                        // or if the collision distance is zero
                        continue;
                    }

                    collisionDist = minDist;

                    lostSoulHit = &lostSoul;
                }
            }
        }
    }

    if(lostSoulHit)
    {
        m_Caretaker->OnLostSoulHit(*this, *lostSoulHit);
    }
}

void
Game::CommitDespawnQueues()
{
    for(const EntityId idToDespawn : m_DespawnQueue)
    {
        for(LostSoul& lostSoul : m_LostSouls)
        {
            if(lostSoul.GetEntityId() == idToDespawn)
            {
                lostSoul = std::move(m_LostSouls.back());
                m_LostSouls.pop_back();
                break;
            }
        }
    }
    m_DespawnQueue.clear();
}
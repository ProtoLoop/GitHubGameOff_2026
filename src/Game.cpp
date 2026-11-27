#include "Game.h"

#include "AudioPlayer.h"
#include "Defer.h"
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

        GG_CHECK(game.QueueSpawnProp(propTypeDefResult->SpriteName, transform),
            "Failed to spawn prop: {}",
            propTypeDefResult->SpriteName);
    }

    for(const ActorInstanceDef& actorDef : levelDef.ActorInstDefs)
    {
        const auto actorTypeDefResult = levelDef.GetActorTypeDef(actorDef.TypeName);
        GG_CHECK(actorTypeDefResult,
            "Failed to get actor type definition for {}",
            actorDef.TypeName);

        if(GG_VERIFYB(actorTypeDefResult->ClassName == "Tombstone",
               "Unknown actor class: {}",
               actorTypeDefResult->ClassName))
        {
            Transform2D transform = actorDef.Transform;

            // Default scale is set on the type definition.
            // It can be overridden by the instance definition.
            transform.Scale = actorDef.OptScale.value_or(actorTypeDefResult->Scale);

            GG_CHECK(game.QueueSpawnTombstone(actorTypeDefResult->SpriteName, transform),
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

    GG_CHECKB(QueueSpawnCaretaker(Transform2D{}), "Failed to spawn caretaker");

    m_Stage = Stage::Running;
    return Result<>::Ok;
}

LostSoul*
Game::TryGetLostSoul(const EntityId entityId)
{
    for(LostSoul& ls : m_LostSouls)
    {
        if(ls.GetEntityId() == entityId)
        {
            return &ls;
        }
    }
    return nullptr;
}

Result<EntityId>
Game::QueueSpawnCaretaker(const Transform2D& transform)
{
    GG_CHECKB(m_Caretakers.size() < m_Caretakers.capacity(),
        "Maximum number of caretakers reached");

    GG_TRYB(caretaker, Caretaker::Create(*this), "Failed to create caretaker");
    caretaker.SetTransform(transform);

    const EntityId entityId = caretaker.GetEntityId();

    m_SpawnQueue.emplace_back(std::move(caretaker));

    return entityId;
}

Result<EntityId>
Game::QueueSpawnProp(const std::string_view spriteName, const Transform2D& transform)
{
    GG_CHECKB(m_Props.size() < m_Props.capacity(), "Maximum number of props reached");

    auto spriteResult = m_Level->GetSprite(spriteName);
    GG_CHECKB(spriteResult, "Failed to get sprite: {}", spriteName);

    auto spriteInstanceResult = spriteResult->CreateInstance();
    GG_CHECKB(spriteInstanceResult, "Failed to create sprite instance for {}", spriteName);

    Prop prop(*spriteInstanceResult, transform);

    const EntityId entityId = prop.GetEntityId();

    m_SpawnQueue.emplace_back(std::move(prop));

    return entityId;
}

Result<EntityId>
Game::QueueSpawnTombstone(const std::string_view spriteName, const Transform2D& transform)
{
    GG_CHECKB(m_Tombstones.size() < m_Tombstones.capacity(),
        "Maximum number of tombstones reached");

    GG_TRYB(tombstone,
        Tombstone::Create(*this, spriteName),
        "Failed to create tombstone: {}",
        spriteName);
    tombstone.SetTransform(transform);

    const EntityId entityId = tombstone.GetEntityId();

    m_SpawnQueue.emplace_back(std::move(tombstone));

    return entityId;
}

Result<EntityId>
Game::QueueSpawnLostSoul(const Transform2D& transform)
{
    GG_CHECKB(m_LostSouls.size() < m_LostSouls.capacity(), "Maximum number of lost souls reached");

    GG_TRYB(lostSoul, LostSoul::Create(*this), "Failed to create lost soul");
    lostSoul.SetTransform(transform);

    const EntityId entityId = lostSoul.GetEntityId();

    m_SpawnQueue.emplace_back(std::move(lostSoul));

    return entityId;
}

Result<>
Game::QueueDespawn(const EntityId entityId)
{
    bool inQueue = false;

    for(const EntityId& id : m_DespawnQueue)
    {
        if(id == entityId)
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
        if(ls.GetEntityId() == entityId)
        {
            isValid = true;
            break;
        }
    }

    GG_CHECKB(isValid, "Unsupported despawn type");

    m_DespawnQueue.push_back(entityId);

    return Result<>::Ok;
}

Result<>
Game::Tick(const float deltaTime)
{
    GG_CHECKB(m_Stage == Stage::Running, "Game is not running");

    GG_CHECK(CommitSpawnQueue());
    CommitDespawnQueues();

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

    CheckCollisions();

    const Caretaker& caretaker = GetCaretaker();

    m_Renderer->SetView(caretaker.GetPos().x, caretaker.GetPos().y);

    GG_CHECK(Render());

    GG_CHECK(m_AudioPlayer.Tick(deltaTime));

    return Result<>::Ok;
}

// private:

Result<>
Game::TickEntities(float deltaTime)
{
    for(Tickable& tickable : m_Tickables)
    {
        GG_CHECK(tickable.Tick(deltaTime, *this));
    }

    return Result<>::Ok;
}

Result<>
Game::AddRenderable(const Renderable& renderable)
{
    GG_CHECKB(m_Renderables.size() < m_Renderables.capacity(), "Max renderables reached");
    m_Renderables.push_back(renderable);
    return Result<>::Ok;
}

Result<>
Game::RemoveRenderable(const EntityId entityId)
{
    const Renderable* found = nullptr;
    for(Renderable& renderable : m_Renderables)
    {
        if(renderable.GetEntityId() == entityId)
        {
            renderable = m_Renderables.back();
            m_Renderables.pop_back();
            found = &renderable;
            break;
        }
    }

    GG_CHECKB(found, "Renderable not found");
    return Result<>::Ok;
}

Result<>
Game::AddTickable(const Tickable& tickable)
{
    GG_CHECKB(m_Tickables.size() < m_Tickables.capacity(), "Max tickables reached");
    m_Tickables.push_back(tickable);
    return Result<>::Ok;
}

Result<>
Game::RemoveTickable(const EntityId entityId)
{
    const Tickable* found = nullptr;
    for(Tickable& tickable : m_Tickables)
    {
        if(tickable.GetEntityId() == entityId)
        {
            tickable = m_Tickables.back();
            m_Tickables.pop_back();
            found = &tickable;
            break;
        }
    }

    GG_CHECKB(found, "Tickable not found");
    return Result<>::Ok;
}

Result<>
Game::AddCollidable(const Collidable& collidable)
{
    GG_CHECKB(m_Collidables.size() < m_Collidables.capacity(), "Max collidables reached");
    m_Collidables.push_back(collidable);
    return Result<>::Ok;
}

Result<>
Game::RemoveCollidable(const EntityId entityId)
{
    const Collidable* found = nullptr;
    for(Collidable& collidable : m_Collidables)
    {
        if(collidable.GetEntityId() == entityId)
        {
            collidable = m_Collidables.back();
            m_Collidables.pop_back();
            found = &collidable;
            break;
        }
    }

    GG_CHECKB(found, "Collidable not found");
    return Result<>::Ok;
}

Result<>
Game::Render()
{
    Renderer& renderer = *m_Renderer;

    for(Renderable& renderable : m_Renderables)
    {
        GG_CHECK(renderable.Render(renderer));

        if(kDrawRectangles)
        {
            const SpriteFrame& spriteFrame = renderable.GetSpriteFrame();
            const Transform2D& transform = renderable.GetTransform();
            QueueFrameRect(spriteFrame, transform);
            QueueCollisionRects(spriteFrame, transform);
        }
    }

    return Result<>::Ok;
}

void
Game::QueueFrameRect(const SpriteFrame& spriteFrame, const Transform2D& transform)
{
    static constexpr glm::vec4 kYellow{ 1, 1, 0, 1 };

    const Rectf drawRect =
        (Rectf{ spriteFrame.GetSize() } - spriteFrame.GetPivot()) * transform.Scale;

    if(!m_Renderer->QueueRectangle(drawRect, transform, kYellow))
    {
        GG_ERROR("Failed to queue rectangle for prop");
    }
}

void
Game::QueueCollisionRects(const SpriteFrame& spriteFrame, const Transform2D& transform)
{
    static constexpr glm::vec4 kMagenta{ 1, 0, 1, 1 };

    for(const Rectf colRect : spriteFrame.GetCollisionRects())
    {
        const Rectf drawRect = colRect * transform.Scale;

        if(!m_Renderer->QueueRectangle(drawRect, transform, kMagenta))
        {
            GG_ERROR("Failed to queue rectangle for prop collision rect");
            break;
        }
    }
}

namespace
{

/// Projects a rectangle from object space to screen space.
Rectf
ProjectRect(const Rectf& rect,
    const Transform2D& transform,
    const glm::mat2x2& proj,
    const glm::vec2 pixelScale)
{
    Rectf rs = rect * transform.Scale;
    rs += proj * transform.Pos * pixelScale;
    return rs;
}

} // namespace

void
Game::CheckCollisions()
{
    const float cellPixPerSide = static_cast<float>(m_Renderer->GetCellPixelsPerSide());

    // Y flip the projected map coords to convert from map space +Y up/North to screen space +Y
    // down.
    const glm::vec2 pixelScale = glm::vec2(1, -1) * cellPixPerSide;

    const glm::mat2x2& proj = m_Renderer->GetProjectionMatrix();

    Caretaker& caretaker = GetCaretaker();

    const Collidable ctCollidable(GetCaretaker());

    // Project collision bounds into screen space
    const Rectf ctBounds = ProjectRect(ctCollidable.GetCollisionBounds(),
        ctCollidable.GetTransform(),
        proj,
        pixelScale);

    for(const Collidable& otherCollidable : m_Collidables)
    {
        // Project the other collidable's collision bounds into screen space
        const Rectf otherBounds = ProjectRect(otherCollidable.GetCollisionBounds(),
            otherCollidable.GetTransform(),
            proj,
            pixelScale);

        // FIXME(KB) - not needed if both colliders only have one collision rect

        // Check the union of the collision bounds first before checking individual rects
        if(!ctBounds.Intersects(otherBounds))
        {
            continue;
        }

        float penDepth = std::numeric_limits<float>::max();
        glm::vec2 collisionNormal;
        // Entity that was hit
        std::optional<EntityId> otherHitId;

        for(const Rectf& ctColRect : ctCollidable.GetCollisionRects())
        {
            const Rectf ctRect =
                ProjectRect(ctColRect, ctCollidable.GetTransform(), proj, pixelScale);

            for(const Rectf& otherColRect : otherCollidable.GetCollisionRects())
            {
                const Rectf otherRect =
                    ProjectRect(otherColRect, otherCollidable.GetTransform(), proj, pixelScale);

                if(ctRect.Intersects(otherRect))
                {
                    // Penetration depth for the collision
                    const float depthR = otherRect.Right() - ctRect.Left();
                    const float depthL = ctRect.Right() - otherRect.Left();
                    const float depthT = ctRect.Bottom() - otherRect.Top();
                    const float depthB = otherRect.Bottom() - ctRect.Top();

                    const float minDepth = std::min({ depthR, depthL, depthT, depthB });
                    if(minDepth <= 0 || minDepth >= penDepth)
                    {
                        // Skip if this is not the closest collision
                        // or if the penetration depth is zero/negative
                        continue;
                    }

                    penDepth = minDepth;
                    if(minDepth == depthR)
                    {
                        // Right collision
                        collisionNormal = glm::vec2(1, 0);
                    }
                    else if(minDepth == depthL)
                    {
                        // Left collision
                        collisionNormal = glm::vec2(-1, 0);
                    }
                    else if(minDepth == depthT)
                    {
                        // Top collision
                        collisionNormal = glm::vec2(0, -1);
                    }
                    else if(minDepth == depthB)
                    {
                        // Bottom collision
                        collisionNormal = glm::vec2(0, 1);
                    }

                    // Unproject the collision normal.
                    collisionNormal = glm::inverse(proj) * collisionNormal;

                    otherHitId = otherCollidable.GetEntityId();
                }
            }
        }

        if(otherHitId)
        {
            caretaker.OnCollided(*this, *otherHitId, collisionNormal, penDepth);
        }
    }
}

Result<>
Game::CommitSpawnQueue()
{
    for(Spawnable& spawnable : m_SpawnQueue)
    {
        auto result = std::visit(
            [this](auto& entity) -> Result<EntityId>
            {
                if constexpr(std::is_same_v<std::decay_t<decltype(entity)>, Caretaker>)
                {
                    auto& newEntity = m_Caretakers.emplace_back(std::move(entity));

                    auto fail = GG_DEFER
                    {
                        m_Caretakers.pop_back();
                    };

                    GG_CHECKB(AddRenderable(Renderable(newEntity)),
                        "Failed to add caretaker to renderables");
                    GG_CHECKB(AddTickable(Tickable(newEntity)),
                        "Failed to add caretaker to tickables");

                    // Don't add the caretaker to collidables.

                    fail.cancel();

                    return newEntity.GetEntityId();
                }
                else if constexpr(std::is_same_v<std::decay_t<decltype(entity)>, Prop>)
                {
                    auto& newEntity = m_Props.emplace_back(std::move(entity));

                    auto fail = GG_DEFER
                    {
                        m_Props.pop_back();
                    };

                    GG_CHECKB(AddRenderable(Renderable(newEntity)),
                        "Failed to add prop to renderables");
                    GG_CHECKB(AddTickable(Tickable(newEntity)), "Failed to add prop to tickables");
                    GG_CHECKB(AddCollidable(Collidable(newEntity)),
                        "Failed to add prop to collidables");

                    fail.cancel();

                    return newEntity.GetEntityId();
                }
                else if constexpr(std::is_same_v<std::decay_t<decltype(entity)>, LostSoul>)
                {
                    auto& newEntity = m_LostSouls.emplace_back(std::move(entity));

                    auto fail = GG_DEFER
                    {
                        m_LostSouls.pop_back();
                    };

                    GG_CHECKB(AddRenderable(Renderable(newEntity)),
                        "Failed to add lost soul to renderables");
                    GG_CHECKB(AddTickable(Tickable(newEntity)),
                        "Failed to add lost soul to tickables");
                    GG_CHECKB(AddCollidable(Collidable(newEntity)),
                        "Failed to add lost soul to collidables");

                    fail.cancel();

                    return newEntity.GetEntityId();
                }
                else if constexpr(std::is_same_v<std::decay_t<decltype(entity)>, Tombstone>)
                {
                    auto& newEntity = m_Tombstones.emplace_back(std::move(entity));

                    auto fail = GG_DEFER
                    {
                        m_Tombstones.pop_back();
                    };

                    GG_CHECKB(AddRenderable(Renderable(newEntity)),
                        "Failed to add tombstone to renderables");
                    GG_CHECKB(AddTickable(Tickable(newEntity)),
                        "Failed to add tombstone to tickables");
                    GG_CHECKB(AddCollidable(Collidable(newEntity)),
                        "Failed to add tombstone to collidables");

                    fail.cancel();

                    return newEntity.GetEntityId();
                }
            },
            spawnable);

        GG_VERIFYB(result, "Failed to spawn entity");
    }
    m_SpawnQueue.clear();
    return Result<>::Ok;
}

void
Game::CommitDespawnQueues()
{
    for(const EntityId idToDespawn : m_DespawnQueue)
    {
        GG_ASSERT_ONLY(bool found = false;)

        // Only handle despawning lost souls for now.
        for(LostSoul& lostSoul : m_LostSouls)
        {
            if(lostSoul.GetEntityId() == idToDespawn)
            {
                GG_ASSERT_ONLY(found = true;);

                GG_VERIFYB(RemoveRenderable(lostSoul.GetEntityId()), "Failed to remove renderable");
                GG_VERIFYB(RemoveTickable(lostSoul.GetEntityId()), "Failed to remove tickable");
                GG_VERIFYB(RemoveCollidable(lostSoul.GetEntityId()), "Failed to remove collidable");

                if(idToDespawn != m_LostSouls.back().GetEntityId())
                {
                    lostSoul = std::move(m_LostSouls.back());

                    auto itRenderable = std::ranges::find(m_Renderables,
                        lostSoul.GetEntityId(),
                        &Renderable::GetEntityId);

                    if(GG_VERIFY(itRenderable != m_Renderables.end(),
                           "Failed to find renderable for lost soul"))
                    {
                        *itRenderable = Renderable(lostSoul);
                    }

                    auto itTickable = std::ranges::find(m_Tickables,
                        lostSoul.GetEntityId(),
                        &Tickable::GetEntityId);

                    if(GG_VERIFY(itTickable != m_Tickables.end(),
                           "Failed to find tickable for lost soul"))
                    {
                        *itTickable = Tickable(lostSoul);
                    }

                    auto itCollidable = std::ranges::find(m_Collidables,
                        lostSoul.GetEntityId(),
                        &Collidable::GetEntityId);

                    if(GG_VERIFY(itCollidable != m_Collidables.end(),
                           "Failed to find collidable for lost soul"))
                    {
                        *itCollidable = Collidable(lostSoul);
                    }
                }

                m_LostSouls.pop_back();
                break;
            }
        }

        GG_ASSERT(found, "Failed to find entity to despawn");
    }
    m_DespawnQueue.clear();
}
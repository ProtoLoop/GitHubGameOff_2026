#pragma once

#include "Actors/Caretaker.h"
#include "Actors/LostSoul.h"
#include "Actors/Tombstone.h"
#include "AudioPlayer.h"
#include "Level.h"
#include "Prop.h"
#include "Random.h"

#include <memory>
#include <variant>

class Input;
class Renderer;
class AudioPlayer;
struct LevelDef;

/// Type-erased wrapper for renderable objects that provides a uniform interface for rendering,
/// allowing objects with a Render method to be managed polymorphically.
class Renderable
{
public:
    template<typename T>
    explicit Renderable(T& object)
        : m_Object(&object),
          m_RenderFunc(RenderImpl<T>),
          m_GetEntityIdFunc(GetEntityIdImpl<T>),
          m_GetSpriteFrameFunc(GetSpriteFrameImpl<T>),
          m_GetTransformFunc(GetTransformImpl<T>)
    {
        static_assert(
            requires(T& t) { t.Render(std::declval<Renderer&>()); },
            "Renderable object must have a Render method with signature Result<> Render(Renderer&)");
    }

    Result<> Render(Renderer& renderer) { return m_RenderFunc(m_Object, renderer); }

    SpriteFrame GetSpriteFrame() const { return m_GetSpriteFrameFunc(m_Object); }

    const Transform2D& GetTransform() const { return m_GetTransformFunc(m_Object); }

    EntityId GetEntityId() const { return m_GetEntityIdFunc(m_Object); }

private:
    const void* m_Object{ nullptr };

    template<typename T>
    static Result<> RenderImpl(const void* obj, Renderer& renderer)
    {
        return static_cast<const T*>(obj)->Render(renderer);
    }

    template<typename T>
    static const Transform2D& GetTransformImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetTransform();
    }

    template<typename T>
    static EntityId GetEntityIdImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetEntityId();
    }

    template<typename T>
    static SpriteFrame GetSpriteFrameImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetSpriteFrame();
    }

    Result<> (*m_RenderFunc)(const void* object, Renderer& renderer);
    EntityId (*m_GetEntityIdFunc)(const void* object);
    SpriteFrame (*m_GetSpriteFrameFunc)(const void* object);
    const Transform2D& (*m_GetTransformFunc)(const void* object);
};

/// Type-erased wrapper for tickable objects that provides a uniform interface for ticking,
/// allowing objects with a Tick method to be managed polymorphically.
class Tickable
{
public:
    template<typename T>
    explicit Tickable(T& object)
        : m_Object(&object),
          m_TickFunc(TickImpl<T>),
          m_GetEntityIdFunc(GetEntityIdImpl<T>)
    {
    }

    Result<> Tick(const float deltaTime, Game& game)
    {
        return m_TickFunc(m_Object, deltaTime, game);
    }

    EntityId GetEntityId() const { return m_GetEntityIdFunc(m_Object); }

private:
    void* m_Object{ nullptr };

    template<typename T>
    static Result<> TickImpl(void* obj, const float deltaTime, Game& game)
    {
        if constexpr(requires(T& object) { object.Tick(deltaTime, game); })
        {
            return static_cast<T*>(obj)->Tick(deltaTime, game);
        }
        else
        {
            return Result<>::Ok;
        }
    }

    template<typename T>
    static EntityId GetEntityIdImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetEntityId();
    }

    Result<> (*m_TickFunc)(void* object, const float deltaTime, Game& game);
    EntityId (*m_GetEntityIdFunc)(const void* object);
};

/// Type-erased wrapper for collision objects that provides a uniform interface for
/// performing collision checks.
class Collidable
{
public:
    template<typename T>
    explicit Collidable(const T& object)
        : m_Object(&object),
          m_GetCollisionBoundsFunc(GetCollisionBoundsImpl<T>),
          m_GetTransformFunc(GetTransformImpl<T>),
          m_GetCollisionRectsFunc(GetCollisionRectsImpl<T>),
          m_GetEntityIdFunc(GetEntityIdImpl<T>)
    {
    }

    Rectf GetCollisionBounds() const { return m_GetCollisionBoundsFunc(m_Object); }

    const Transform2D& GetTransform() const { return m_GetTransformFunc(m_Object); }

    std::span<const Rectf> GetCollisionRects() const { return m_GetCollisionRectsFunc(m_Object); }

    EntityId GetEntityId() const { return m_GetEntityIdFunc(m_Object); }

private:
    const void* m_Object{ nullptr };

    template<typename T>
    static Rectf GetCollisionBoundsImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetCollisionBounds();
    }

    template<typename T>
    static const Transform2D& GetTransformImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetTransform();
    }

    template<typename T>
    static std::span<const Rectf> GetCollisionRectsImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetCollisionRects();
    }

    template<typename T>
    static EntityId GetEntityIdImpl(const void* obj)
    {
        return static_cast<const T*>(obj)->GetEntityId();
    }

    Rectf (*m_GetCollisionBoundsFunc)(const void* object);
    const Transform2D& (*m_GetTransformFunc)(const void* object);
    std::span<const Rectf> (*m_GetCollisionRectsFunc)(const void* object);
    EntityId (*m_GetEntityIdFunc)(const void* object);
};

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
    Game(Game&&) = default;
    Game& operator=(Game&&) = default;

    static Result<Game> Create(Input& input,
        const LevelDef& levelDef,
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

    LostSoul* TryGetLostSoul(const EntityId entityId);

    Random& GetEntityRng() { return m_EntityRng; }

    Result<EntityId> QueueSpawnCaretaker(const Transform2D& transform);

    Result<EntityId> QueueSpawnProp(const std::string_view spriteName,
        const Transform2D& transform);

    Result<EntityId> QueueSpawnTombstone(const std::string_view spriteName,
        const Transform2D& transform);

    Result<EntityId> QueueSpawnLostSoul(const Transform2D& transform);

    Result<> QueueDespawn(const EntityId entityId);

    Result<> Tick(const float deltaTime);

    bool IsRunning() const { return m_Stage == Stage::Running; }

private:
    static constexpr size_t kMaxCareTakers = 1;
    static constexpr size_t kMaxLostSouls = 16;
    static constexpr size_t kMaxTombstones = kMaxLostSouls;
    static constexpr size_t kMaxProps = 128;
    static constexpr size_t kMaxEntities =
        kMaxCareTakers + kMaxProps + kMaxLostSouls + kMaxTombstones;

    enum class Stage
    {
        None,
        Running,
    };

    Game(Input& input,
        std::unique_ptr<Level>&& level,
        std::unique_ptr<Renderer>&& renderer,
        AudioPlayer&& audioPlayer);

    Result<> TickEntities(float deltaTime);

    Caretaker& GetCaretaker() { return m_Caretakers[0]; }
    const Caretaker& GetCaretaker() const { return m_Caretakers[0]; }

    Result<> AddRenderable(const Renderable& renderable);
    Result<> RemoveRenderable(const EntityId entityId);

    Result<> AddTickable(const Tickable& tickable);
    Result<> RemoveTickable(const EntityId entityId);

    Result<> AddCollidable(const Collidable& collidable);
    Result<> RemoveCollidable(const EntityId entityId);

    Result<> Render();

    void QueueFrameRect(const SpriteFrame& spriteFrame, const Transform2D& transform);

    void QueueCollisionRects(const SpriteFrame& spriteFrame, const Transform2D& transform);

    void CheckCollisions();

    Result<> CommitSpawnQueue();

    void CommitDespawnQueues();

    Stage m_Stage{ Stage::None };

    Input* m_Input;
    std::unique_ptr<Level> m_Level;
    std::unique_ptr<Renderer> m_Renderer;
    AudioPlayer m_AudioPlayer;

    BoundedVector<Caretaker> m_Caretakers{ kMaxCareTakers };
    BoundedVector<Prop> m_Props{ kMaxProps };
    BoundedVector<LostSoul> m_LostSouls{ kMaxLostSouls };
    BoundedVector<Tombstone> m_Tombstones{ kMaxTombstones };

    using Spawnable = std::variant<Caretaker, Prop, LostSoul, Tombstone>;

    BoundedVector<Spawnable> m_SpawnQueue{ kMaxEntities };

    BoundedVector<EntityId> m_DespawnQueue{ kMaxEntities };

    BoundedVector<Renderable> m_Renderables{ kMaxEntities };
    BoundedVector<Tickable> m_Tickables{ kMaxEntities };
    BoundedVector<Collidable> m_Collidables{ kMaxEntities };

    float m_TimeAccum{ 0.0f };

    static constexpr uint64_t EntityRngSeed = 42;
    static constexpr uint64_t EntityRngStream = 54;
    Random m_EntityRng{ EntityRngSeed, EntityRngStream };
};
#pragma once

#include "EntityId.h"
#include "Sprite.h"
#include "Timer.h"
#include "Transform2D.h"

#include <optional>

class Game;
class Renderer;
class AudioClip;

class Tombstone : public Transform2DLike<Tombstone>
{
public:
    Tombstone() = delete;
    ~Tombstone() = default;
    Tombstone(const Tombstone&) = delete;
    Tombstone& operator=(const Tombstone&) = delete;
    Tombstone(Tombstone&&) = default;
    Tombstone& operator=(Tombstone&&) = default;

    static Result<Tombstone> Create(Game& game, const std::string_view spriteName);

    EntityId GetEntityId() const { return m_Id; }

    SpriteFrame GetSpriteFrame() const { return m_CurSprite.GetFrame(); }

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer) const;

    Rectf GetCollisionBounds() const { return GetSpriteFrame().GetCollisionBounds(); }

    std::span<const Rectf> GetCollisionRects() const
    {
        return GetSpriteFrame().GetCollisionRects();
    }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    /// Called by the caretaker when a lost soul associated with this tombstone is collected.
    void OnLostSoulCollected(Game& game, const EntityId lostSoulId);

    /// Called by the caretaker when a lost soul associated with this tombstone is dropped off.
    void OnLostSoulDroppedOff(Game& game, const EntityId lostSoulId);

private:

    enum class State
    {
        Init,
        WaitingToSpawn,
        WaitingForCollection,
        WaitingForDropoff,
        Dormant,
    };

    struct AudioClips
    {
        const AudioClip* LostSoulSpawn;
    };

    explicit Tombstone(const Sprite::Instance& curSprite, const AudioClips& audioClips);

    void EnterWaitingToSpawn(Game& game);
    void EnterWaitingForCollection(Game& game);
    void EnterWaitingForDropoff(Game& game);
    void EnterDormant(Game& game);

    void TickWaitingToSpawn(const float deltaTime, Game& game);
    void TickWaitingForCollection(const float deltaTime, Game& game);
    void TickWaitingForDropoff(const float deltaTime, Game& game);
    void TickDormant(const float deltaTime, Game& game);

    EntityId m_Id{ EntityId::NextId() };

    CountdownTimer m_Timer;

    Transform2D m_Transform;

    std::optional<EntityId> m_LostSoulId;

    /// Tracks the current cycle of the shimmer effect for the tombstone.
    float m_ShimmerCycle{ 0.0f };

    Sprite::Instance m_CurSprite;
    AudioClips m_AudioClips;
    State m_State{ State::Init };
};
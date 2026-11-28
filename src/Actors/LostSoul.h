#pragma once

#include "EntityId.h"
#include "Sprite.h"
#include "Timer.h"
#include "Transform2D.h"

class Game;
class Renderer;

class LostSoul : public Transform2DLike<LostSoul>
{
public:
    LostSoul() = delete;
    ~LostSoul() = default;
    LostSoul(const LostSoul&) = delete;
    LostSoul& operator=(const LostSoul&) = delete;
    LostSoul(LostSoul&&) = default;
    LostSoul& operator=(LostSoul&&) = default;

    static Result<LostSoul> Create(Game& game, const EntityId tombstoneEntityId);

    EntityId GetEntityId() const { return m_EntityId; }

    /// Returns the ID of the tombstone associated with this lost soul.
    EntityId GetTombstoneId() const { return m_TombstoneId; }

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

private:

    enum class State
    {
        Init,
        FlashSpawn,
        FadingIn,
        Idle,
        Moving,
        Turning,
    };

    void EnterFlashSpawn(Game& game);
    void EnterFadingIn(Game& game);
    void EnterIdle(Game& game);
    void EnterMoving(Game& game);
    void EnterTurning(Game& game);

    void TickFlashSpawn(const float deltaTime, Game& game);
    void TickFadingIn(const float deltaTime, Game& game);
    void TickIdle(const float deltaTime, Game& game);
    void TickMoving(const float deltaTime, Game& game);
    void TickTurning(const float deltaTime, Game& game);

    struct Sprites
    {
        Sprite::Instance Hover;
    };

    LostSoul(const Sprites& sprites, const EntityId tombstoneEntityId);

    EntityId m_EntityId{ EntityId::NextId() };

    // The ID of the tombstone associated with this lost soul.
    EntityId m_TombstoneId;

    CountdownTimer m_Timer;

    Transform2D m_Transform;

    float m_TurnDir{ 1.0f };

    Sprites m_Sprites;
    Sprite::Instance m_CurSprite;
    State m_State{ State::Init };
};
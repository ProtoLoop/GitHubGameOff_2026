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

    static Result<LostSoul> Create(Game& game);

    EntityId GetEntityId() const { return m_Id; }

    SpriteFrame GetSpriteFrame() const { return m_CurSprite.GetFrame(); }

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer) const;

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

private:

    enum class State
    {
        Init,
        Idle,
        Moving,
        Turning,
    };

    void EnterIdle(Game& game);
    void EnterMoving(Game& game);
    void EnterTurning(Game& game);

    void TickIdle(const float deltaTime, Game& game);
    void TickMoving(const float deltaTime, Game& game);
    void TickTurning(const float deltaTime, Game& game);

    struct Sprites
    {
        Sprite::Instance Hover;
    };

    explicit LostSoul(const Sprites& sprites);

    EntityId m_Id{ EntityId::NextId() };

    CountdownTimer m_Timer;

    Transform2D m_Transform;

    float m_TurnDir{ 1.0f };

    Sprites m_Sprites;
    Sprite::Instance m_CurSprite;
    State m_State{ State::Init };
};
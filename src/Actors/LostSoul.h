#pragma once

#include "Sprite.h"
#include "Timer.h"
#include "Transform2D.h"

class Game;

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

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer);

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

    struct Anims
    {
        SpriteAnim::Instance Idle;
        SpriteAnim::Instance Run;
    };

    explicit LostSoul(const Anims& anims);

    CountdownTimer m_Timer;

    Transform2D m_Transform;

    float m_TurnDir{ 1.0f };

    SpriteAnim::Instance m_CurAnim;
    Anims m_Anims;
    State m_State{ State::Init };
};
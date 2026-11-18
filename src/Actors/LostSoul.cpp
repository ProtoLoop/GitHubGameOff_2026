#include "LostSoul.h"

#include "Resources/AnimResources.h"
#include "Game.h"
#include "Renderer.h"

namespace
{
constexpr float kTurnsPerSec = 0.5;
constexpr float kMoveSpeed = 0.75f;
constexpr float kAnimPeriod = 2.0f;

// Offset to align the sprite with the isometric view.
const Angle kIsoAngleOffset(0.125f);

} // namespace

LostSoul::LostSoul(const Anims& anims)
    : m_CurAnim(anims.Idle),
      m_Anims(anims)
{
}

Result<LostSoul>
LostSoul::Create(Game& game)
{
    const Level& level = game.GetLevel();

    GG_TRY(idleAnim,
        level.CreateSpriteAnimInstance(AnimResources::kLS_AnimName_Idle),
        "Failed to create sprite anim instance for {}",
        AnimResources::kLS_AnimName_Idle);

    GG_TRY(walkAnim,
        level.CreateSpriteAnimInstance(AnimResources::kLS_AnimName_Walk),
        "Failed to create sprite anim instance for {}",
        AnimResources::kLS_AnimName_Walk);

    return LostSoul(Anims{ .Idle = idleAnim, .Walk = walkAnim });
}

Result<>
LostSoul::Tick(const float deltaTime, Game& game)
{
    switch(m_State)
    {
        case State::Init:
            EnterIdle(game);
            break;
        case State::Idle:
            TickIdle(deltaTime, game);
            break;
        case State::Moving:
            TickMoving(deltaTime, game);
            break;
        case State::Turning:
            TickTurning(deltaTime, game);
            break;
    }

    m_CurAnim.SetPeriod(kAnimPeriod);
    m_CurAnim.SetAngle(GetRot() + kIsoAngleOffset);
    m_CurAnim.Tick(deltaTime);
    m_Timer.Tick(deltaTime);

    return Result<>::Ok;
}

Result<>
LostSoul::Render(Renderer& renderer) const
{
    GG_CHECKB(renderer.QueueSprite(m_CurAnim.GetFrame(), GetTransform()), "Failed to queue sprite");

    return Result<>::Ok;
}

// private:

void
LostSoul::EnterIdle(Game& game)
{
    constexpr float kMinIdleTime = 1.0f;
    constexpr float kMaxIdleTime = 3.0f;

    const float idleTime = game.GetEntityRng().NextInRange(kMinIdleTime, kMaxIdleTime);

    m_State = State::Idle;
    m_Timer.Start(idleTime);
    m_CurAnim = m_Anims.Idle;
}

void
LostSoul::EnterMoving(Game& game)
{
    constexpr float kMinMoveTime = 1.0f;
    constexpr float kMaxMoveTime = 3.0f;

    const float moveTime = game.GetEntityRng().NextInRange(kMinMoveTime, kMaxMoveTime);

    m_State = State::Moving;
    m_Timer.Start(moveTime);
    m_CurAnim = m_Anims.Walk;
}

void
LostSoul::EnterTurning(Game& game)
{
    constexpr float kMinTurnTime = 0.1f;
    constexpr float kMaxTurnTime = 0.3f;

    const float turnTime = game.GetEntityRng().NextInRange(kMinTurnTime, kMaxTurnTime);

    m_TurnDir = game.GetEntityRng().NextBool(0.5f) ? 1.0f : -1.0f;

    m_State = State::Turning;
    m_Timer.Start(turnTime);

    // Keep the current animation as is for turning.
}

void
LostSoul::TickIdle(const float /*deltaTime*/, Game& game)
{
    if(m_Timer.IsExpired())
    {
        if(game.GetEntityRng().NextBool(0.5f))//NOLINT
        {
            EnterMoving(game);
        }
        else
        {
            EnterTurning(game);
        }
        return;
    }
}

void
LostSoul::TickMoving(const float deltaTime, Game& game)
{
    if(m_Timer.IsExpired())
    {
        if(game.GetEntityRng().NextBool(0.3f))//NOLINT
        {
            EnterIdle(game);
        }
        else
        {
            EnterTurning(game);
        }
        return;
    }

    MoveBy(GetForward() * kMoveSpeed * deltaTime);
}

void
LostSoul::TickTurning(const float deltaTime, Game& game)
{
    if(m_Timer.IsExpired())
    {
        if(game.GetEntityRng().NextBool(0.3f))//NOLINT
        {
            EnterIdle(game);
        }
        else
        {
            EnterMoving(game);
        }
        return;
    }

    RotateBy(Angle(m_TurnDir * kTurnsPerSec * deltaTime));
}
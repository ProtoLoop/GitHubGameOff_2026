#include "LostSoul.h"

#include "Game.h"
#include "Renderer.h"

#include <string_view>

namespace
{
constexpr float kTurnsPerSec = 0.5;
constexpr float kMoveSpeed = 0.5f;
constexpr float kAnimPeriod = 1.0f;

// Offset to align the sprite with the isometric view.
const Angle kIsoAngleOffset(0.125f);

constexpr const std::string_view kSpriteName_hover{"SH_Hover"};

} // namespace

LostSoul::LostSoul(const Sprites& sprites)
    : m_Sprites(sprites),
      m_CurSprite(m_Sprites.Hover)
{
}

Result<LostSoul>
LostSoul::Create(Game& game)
{
    const Level& level = game.GetLevel();

    GG_TRY(hoverSprite,
        level.CreateSpriteInstance(kSpriteName_hover),
        "Failed to create sprite instance for {}",
        kSpriteName_hover);

    return LostSoul(Sprites{ .Hover = hoverSprite });
}

Result<>
LostSoul::Tick(const float deltaTime, Game& game)
{
    switch(m_State)
    {
        case State::Init:
            EnterIdle(game);
            m_CurSprite.SetPhase(game.GetEntityRng().NextFloat());
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

    m_CurSprite.SetPeriod(kAnimPeriod);
    m_CurSprite.SetAngle(GetRot());
    m_CurSprite.Tick(deltaTime);
    m_Timer.Tick(deltaTime);

    return Result<>::Ok;
}

Result<>
LostSoul::Render(Renderer& renderer) const
{
    // If in isometric view then apply the isometric angle offset to the sprite.
    const Angle angleOffset = renderer.GetProjectionType() == Renderer::ProjectionType::Isometric
        ? kIsoAngleOffset
        : Angle(0.0f);
        
    const SpriteFrame spriteFrame =
        m_CurSprite.GetFrame(m_CurSprite.GetPhase(), m_CurSprite.GetAngle() + angleOffset);
    GG_CHECKB(renderer.QueueSprite(spriteFrame, GetTransform()), "Failed to queue sprite");

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
    const float phase = m_CurSprite.GetPhase();
    m_CurSprite = m_Sprites.Hover;
    m_CurSprite.SetPhase(phase);
}

void
LostSoul::EnterMoving(Game& game)
{
    constexpr float kMinMoveTime = 1.0f;
    constexpr float kMaxMoveTime = 3.0f;

    const float moveTime = game.GetEntityRng().NextInRange(kMinMoveTime, kMaxMoveTime);

    m_State = State::Moving;
    m_Timer.Start(moveTime);
    const float phase = m_CurSprite.GetPhase();
    m_CurSprite = m_Sprites.Hover;
    m_CurSprite.SetPhase(phase);
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
        if(game.GetEntityRng().NextBool(0.5f)) // NOLINT
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
        if(game.GetEntityRng().NextBool(0.3f)) // NOLINT
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
        if(game.GetEntityRng().NextBool(0.3f)) // NOLINT
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
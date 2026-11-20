#include "Caretaker.h"

#include "Game.h"
#include "Input.h"
#include "Renderer.h"
#include "Resources/AnimResources.h"

namespace
{
constexpr float kTurnsPerSec = 1;
constexpr float kMoveSpeed = 2.5f;
constexpr float kAnimPeriod_Run = 0.66f;
constexpr float kAnimPeriod_Idle = 2.0f;

// Offset to align the sprite with the isometric view.
const Angle kIsoAngleOffset(0.125f);

constexpr ResourceName kAnimName_Idle = AnimResources::kWarrior_AnimName_Idle;
constexpr ResourceName kAnimName_Run = AnimResources::kWarrior_AnimName_Run;

} // namespace

Caretaker::Caretaker(const Anims& anims)
    : m_Anims(anims),
      m_CurAnim(m_Anims.Idle),
      m_CurAnimPeriod(kAnimPeriod_Idle)
{
}

Result<Caretaker>
Caretaker::Create(Game& game)
{
    const Level& level = game.GetLevel();

    GG_TRY(idleAnim,
        level.CreateSpriteAnimInstance(kAnimName_Idle),
        "Failed to create sprite anim instance for {}",
        kAnimName_Idle);

    GG_TRY(runAnim,
        level.CreateSpriteAnimInstance(kAnimName_Run),
        "Failed to create sprite anim instance for {}",
        kAnimName_Run);

    return Caretaker(Anims{ .Idle = idleAnim, .Run = runAnim });
}

Result<>
Caretaker::Tick(const float deltaTime, Game& game)
{
    const Input& input = game.GetInput();

    float value{};
    if(input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        RotateBy(Angle(-kTurnsPerSec * deltaTime));
    }
    if(input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        RotateBy(Angle(kTurnsPerSec * deltaTime));
    }

    const glm::vec2& forward = GetForward();
    const glm::vec2& right = GetRight();

    bool isMoving = false;
    const float dmove = kMoveSpeed * deltaTime;

    if(input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        MoveBy(-right * value * dmove);
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        MoveBy(right * value * dmove);
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        MoveBy(forward * value * dmove);
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        MoveBy(-forward * value * dmove);
        isMoving = true;
    }

    const SpriteAnim::Instance& desiredAnim = isMoving ? m_Anims.Run : m_Anims.Idle;

    if(m_CurAnim.GetName() != desiredAnim.GetName())
    {
        m_CurAnim = desiredAnim;
        m_CurAnim.SetPhase(0);
        m_CurAnimPeriod = isMoving ? kAnimPeriod_Run : kAnimPeriod_Idle;
    }

    m_CurAnim.SetPeriod(m_CurAnimPeriod);
    m_CurAnim.SetAngle(GetRot() + kIsoAngleOffset);
    m_CurAnim.Tick(deltaTime);

    return Result<>::Ok;
}

Result<>
Caretaker::Render(Renderer& renderer) const
{
    const Sprite sprite = m_CurAnim.GetFrame();
    GG_CHECKB(renderer.QueueSprite(sprite, GetTransform()), "Failed to queue sprite");

    return Result<>::Ok;
}

Rect
Caretaker::GetBounds() const
{
    return m_CurAnim.GetFrame().GetRect();
}
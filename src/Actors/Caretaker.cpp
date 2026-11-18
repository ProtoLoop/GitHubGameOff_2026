#include "Caretaker.h"

#include "Resources/AnimResources.h"
#include "Input.h"
#include "Game.h"
#include "Renderer.h"

namespace
{
constexpr float kTurnsPerSec = 1;
constexpr float kMoveSpeed = 2.0f;
constexpr float kAnimPeriod = 0.66f;

// Offset to align the sprite with the isometric view.
const Angle kIsoAngleOffset(0.125f);

} // namespace

Caretaker::Caretaker(const SpriteAnim::Instance& initialAnim)
    : m_CurAnim(initialAnim)
{
}

Result<Caretaker>
Caretaker::Create(Game& game)
{
    GG_TRY(initialAnim,
        game.GetLevel().CreateSpriteAnimInstance(AnimResources::kCT_AnimName_Idle),
        "Failed to create sprite anim instance for {}",
        AnimResources::kCT_AnimName_Idle);

    return Caretaker(initialAnim);
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

    const std::string_view desiredAnimName =
        isMoving ? AnimResources::kCT_AnimName_Run : AnimResources::kCT_AnimName_Idle;

    if(m_CurAnim.GetName() != desiredAnimName)
    {
        GG_TRYB(nextAnim,
            game.GetLevel().CreateSpriteAnimInstance(desiredAnimName),
            "Failed to create sprite anim instance for {}",
            desiredAnimName);
        m_CurAnim = nextAnim;
    }

    m_CurAnim.SetPeriod(kAnimPeriod);
    m_CurAnim.SetAngle(GetRot() + kIsoAngleOffset);
    m_CurAnim.Tick(deltaTime);

    return Result<>::Ok;
}

Result<>
Caretaker::Render(Renderer& renderer) const
{
    GG_CHECKB(renderer.QueueSprite(m_CurAnim.GetFrame(), GetTransform()), "Failed to queue sprite");

    return Result<>::Ok;
}
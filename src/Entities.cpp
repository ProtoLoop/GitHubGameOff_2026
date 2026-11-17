#include "Entities.h"

#include "Input.h"
#include "Level.h"
#include "Renderer.h"

namespace
{
constexpr const ResourceName kAnimName_Idle = "CT_Idle";
constexpr const ResourceName kAnimName_Run = "CT_Run";

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
Caretaker::Create(Level& level)
{
    GG_TRY(initialAnim,
        level.CreateSpriteAnimInstance(kAnimName_Idle),
        "Failed to create sprite anim instance for {}",
        kAnimName_Idle);

    return Caretaker(initialAnim);
}

Result<>
Caretaker::Tick(const float deltaTime, const Input& input, const Level& level)
{
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

    const std::string_view desiredAnimName = isMoving ? kAnimName_Run : kAnimName_Idle;

    if(m_CurAnim.GetName() != desiredAnimName)
    {
        GG_TRYB(nextAnim,
            level.CreateSpriteAnimInstance(desiredAnimName),
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
Caretaker::Render(Renderer& renderer)
{
    GG_CHECKB(renderer.QueueSprite(m_CurAnim.GetFrame(), GetTransform()), "Failed to queue sprite");

    return Result<>::Ok;
}
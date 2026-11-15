#include "Entities.h"

#include "Input.h"
#include "Level.h"
#include "Nodes.h"
#include "Result.h"

#include <glm/vec2.hpp>

namespace
{
constexpr const ResourceName kAnimName_Idle = "CtIdle";
constexpr const ResourceName kAnimName_Run = "CtRun";

constexpr float kTurnsPerSec = 1;
constexpr float kMoveSpeed = 2.0f;
constexpr float kAnimPeriod = 0.66f;

// Offset to align the sprite with the isometric view.
const Angle kIsoAngleOffset(0.125f);

} // namespace

Result<Caretaker>
Caretaker::Create(Level& level)
{
    constexpr std::string_view animNames[]//
    {
        kAnimName_Idle,
        kAnimName_Run
    };

    auto animNodeResult = level.CreateSpriteAnimNode(animNames);
    GG_CHECK(animNodeResult, "Failed to create sprite animation node");

    animNodeResult->SetPeriod(kAnimPeriod);

    return Caretaker(*animNodeResult);
}

Result<>
Caretaker::Tick(const float deltaTime, const Input& input, const Level& /*level*/)
{
    const float dturn = kTurnsPerSec * deltaTime;

    float value{};
    if(input.IsTriggered(Input::ActionId::RotateLeft, value))
    {
        RotateBy(Angle(-dturn));
    }
    if(input.IsTriggered(Input::ActionId::RotateRight, value))
    {
        RotateBy(Angle(dturn));
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

    if(m_AnimNode->WhatsPlaying() != desiredAnimName)
    {
        GG_CHECKB(m_AnimNode->Play(desiredAnimName), "Failed to play animation: {}", desiredAnimName);
        m_AnimNode->SetPeriod(kAnimPeriod);
    }

    m_AnimNode->FaceDirection(GetRot() + kIsoAngleOffset);

    return Result<>::Ok;
}
#include "Caretaker.h"

#include "Actors/Tombstone.h"
#include "AudioPlayer.h"
#include "Game.h"
#include "Input.h"
#include "Renderer.h"

#include <string_view>

namespace
{
constexpr float kTurnsPerSec = 1;
constexpr float kMoveSpeed = 2.5f;
constexpr float kAnimPeriod_Run = 0.66f;
constexpr float kAnimPeriod_Idle = 2.0f;

// Offset to align the sprite with the isometric view.
const Angle kIsoAngleOffset(0.125f);

constexpr const std::string_view kSpriteName_Idle{ "Warrior_Idle" };
constexpr const std::string_view kSpriteName_Run{ "Warrior_Run" };

constexpr const std::string_view kAudioClipName_LostSoulCollect{ "LostSoulCollect" };
constexpr const std::string_view kAudioClipName_LostSoulDropoff{ "LostSoulDropoff" };
} // namespace

Caretaker::Caretaker(const Sprites& sprites, const AudioClips& audioClips)
    : m_Sprites(sprites),
      m_AudioClips(audioClips),
      m_CurSprite(m_Sprites.Idle),
      m_CurSpritePeriod(kAnimPeriod_Idle)
{
}

Result<Caretaker>
Caretaker::Create(Game& game)
{
    const Level& level = game.GetLevel();

    GG_TRY(idleSprite,
        level.CreateSpriteInstance(kSpriteName_Idle),
        "Failed to create sprite anim instance for {}",
        kSpriteName_Idle);

    GG_TRY(runSprite,
        level.CreateSpriteInstance(kSpriteName_Run),
        "Failed to create sprite anim instance for {}",
        kSpriteName_Run);

    GG_TRY(clipLostSoulCollect,
        level.GetAudioClip(kAudioClipName_LostSoulCollect),
        "Failed to load audio clip: {}",
        kAudioClipName_LostSoulCollect);

    GG_TRY(clipLostSoulDropoff,
        level.GetAudioClip(kAudioClipName_LostSoulDropoff),
        "Failed to load audio clip: {}",
        kAudioClipName_LostSoulDropoff);

    const AudioClips audioClips //
        {
            .LostSoulCollect = &clipLostSoulCollect,
            .LostSoulDropoff = &clipLostSoulDropoff,
        };

    return Caretaker(Sprites{ .Idle = idleSprite, .Run = runSprite }, audioClips);
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

    bool isMoving = false;
    m_Velocity = glm::vec2(0.0f);

    if(input.IsTriggered(Input::ActionId::MoveLeft, value))
    {
        m_Velocity -= glm::vec2(1, 0) * value * kMoveSpeed;
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::MoveRight, value))
    {
        m_Velocity += glm::vec2(1, 0) * value * kMoveSpeed;
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::MoveForward, value))
    {
        m_Velocity += forward * value * kMoveSpeed;
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::MoveBackward, value))
    {
        m_Velocity -= forward * value * kMoveSpeed;
        isMoving = true;
    }
    if(input.IsTriggered(Input::ActionId::DebugDraw, value))
    {
        game.ToggleDebugDraw();
    }

    MoveBy(m_Velocity * deltaTime);

    const Sprite::Instance& desiredSprite = isMoving ? m_Sprites.Run : m_Sprites.Idle;

    if(m_CurSprite.GetName() != desiredSprite.GetName())
    {
        m_CurSprite = desiredSprite;
        m_CurSprite.SetPhase(0);
        m_CurSpritePeriod = isMoving ? kAnimPeriod_Run : kAnimPeriod_Idle;
    }

    m_CurSprite.SetPeriod(m_CurSpritePeriod);
    m_CurSprite.SetAngle(GetRot());
    m_CurSprite.Tick(deltaTime);

    return Result<>::Ok;
}

Result<>
Caretaker::Render(Renderer& renderer) const
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

void
Caretaker::OnCollided(Game& game,
    const EntityId entityId,
    const glm::vec2& /*collisionNormal*/,
    const float /*penDepth*/)
{
    // If collided with the tombstone that owns our lost soul then drop it off.
    if(m_TombstoneId.has_value() && entityId == m_TombstoneId.value())
    {
        // Notify the tombstone that its lost soul has been dropped off.
        Tombstone* ts = game.TryGetTombstone(entityId);
        if(GG_VERIFYB(ts, "Failed to get tombstone for lost soul"))
        {
            ts->OnLostSoulDroppedOff(game, *m_LostSoulId);
        }

        // Play the dropoff sound.
        if(!game.GetAudioPlayer().Play(*m_AudioClips.LostSoulDropoff))
        {
            GG_ERROR("Failed to play audio clip: {} - {}",
                m_AudioClips.LostSoulDropoff->GetName(),
                SDL_GetError());
        }
        m_LostSoulId.reset();
        m_TombstoneId.reset();
    }
    else if(!m_LostSoulId.has_value())
    {
        // If collided with a lost soul and we are not already carrying one, attempt to collect it.

        const LostSoul* ls = game.TryGetLostSoul(entityId);

        if(!ls)
        {
            return;
        }

        // Play the collection sound.
        if(!game.GetAudioPlayer().Play(*m_AudioClips.LostSoulCollect))
        {
            GG_ERROR("Failed to play audio clip: {} - {}",
                m_AudioClips.LostSoulCollect->GetName(),
                SDL_GetError());
        }

        // Despawn the lost soul.
        if(!GG_VERIFYB(game.QueueDespawn(entityId), "Failed to queue despawn for lost soul"))
        {
            return;
        }

        // Notify the tombstone that its lost soul has been collected.
        Tombstone* ts = game.TryGetTombstone(ls->GetTombstoneId());
        if(!GG_VERIFYB(ts, "Failed to get tombstone for lost soul"))
        {
            return;
        }

        m_LostSoulId = entityId;
        m_TombstoneId = ts->GetEntityId();
        ts->OnLostSoulCollected(game, *m_LostSoulId);
    }
}
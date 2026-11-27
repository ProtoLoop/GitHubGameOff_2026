#include "Tombstone.h"

#include "Colors.h"
#include "Game.h"
#include "Renderer.h"

#include <numbers>

namespace
{

constexpr const std::string_view kAudioClipName_LostSoulSpawn{ "LostSoulSpawn" };

constexpr float kMinLostSoulSpawnWait = 1.0f;
constexpr float kMaxLostSoulSpawnWait = 15.0f;
constexpr float kLostSoulSpawnOffsetY = 0.5f;

constexpr float kShimmerPeriod = 2;

} // namespace

Tombstone::Tombstone(const Sprite::Instance& curSprite, const AudioClips& audioClips)
    : m_CurSprite(curSprite),
      m_AudioClips(audioClips)
{
}

Result<Tombstone>
Tombstone::Create(Game& game, const std::string_view spriteName)
{
    const Level& level = game.GetLevel();

    GG_TRY(spriteInstance,
        level.CreateSpriteInstance(spriteName),
        "Failed to create sprite instance for {}",
        spriteName);

    GG_TRY(clipLostSoulSpawn,
        level.GetAudioClip(kAudioClipName_LostSoulSpawn),
        "Failed to load audio clip: {}", kAudioClipName_LostSoulSpawn);

    const AudioClips audioClips //
        {
            .LostSoulSpawn = &clipLostSoulSpawn,
        };

    return Tombstone(spriteInstance, audioClips);
}

Result<>
Tombstone::Tick(const float deltaTime, Game& game)
{
    switch(m_State)
    {
        case State::Init:
            EnterWaitingToSpawn(game);
            m_CurSprite.SetPhase(game.GetEntityRng().NextFloat());
            break;
        case State::WaitingToSpawn:
            TickWaitingToSpawn(deltaTime, game);
            break;
        case State::WaitingForDropoff:
            TickWaitingForDropoff(deltaTime, game);
            break;
        case State::Dormant:
            TickDormant(deltaTime, game);
            break;
    }

    m_ShimmerCycle += deltaTime;

    m_ShimmerCycle = fmod(m_ShimmerCycle, kShimmerPeriod);    

    m_Timer.Tick(deltaTime);

    return Result<>::Ok;
}

Result<>
Tombstone::Render(Renderer& renderer) const
{
    SpriteFrame spriteFrame =
        m_CurSprite.GetFrame(m_CurSprite.GetPhase(), m_CurSprite.GetAngle());

    const float shimmer =
        (std::cos(m_ShimmerCycle * 2.0f * std::numbers::pi_v<float> / kShimmerPeriod) + 1)
        * 0.5f;

    const glm::vec4 tint = spriteFrame.GetTint() + (Colors::kEctoplasmGreen * 5.0f * shimmer);

    spriteFrame.SetTint(tint);

    GG_CHECKB(renderer.QueueSprite(spriteFrame, GetTransform()), "Failed to queue sprite");

    return Result<>::Ok;
}

// private:

namespace
{
void
StartSpawnTimer(Game& game, CountdownTimer& timer)
{
    const float duration =
        game.GetEntityRng().NextInRange(kMinLostSoulSpawnWait, kMaxLostSoulSpawnWait);
    timer.Start(duration);
}
} // namespace

void
Tombstone::EnterWaitingToSpawn(Game& game)
{
    StartSpawnTimer(game, m_Timer);
    m_State = State::WaitingToSpawn;
}

void
Tombstone::EnterWaitingForDropoff(Game& /*game*/)
{
    m_State = State::WaitingForDropoff;
}

void
Tombstone::EnterDormant(Game& /*game*/)
{

    m_State = State::Dormant;
}

void
Tombstone::TickWaitingToSpawn(const float /*deltaTime*/, Game& game)
{
    if(m_Timer.IsExpired())
    {
        Transform2D transform = GetTransform();
        transform.Pos.y -= kLostSoulSpawnOffsetY;
        transform.Scale = 1;    //FIXME(KB) - get the scale from the lost soul actor type definition.
        auto entityIdResult = game.QueueSpawnLostSoul(transform);
        if(!GG_VERIFYB(entityIdResult, "Failed to spawn lost soul"))
        {
            // Try again
            StartSpawnTimer(game, m_Timer);
            return;
        }

        GG_VERIFYB(game.GetAudioPlayer().Play(*m_AudioClips.LostSoulSpawn),
            "Failed to play audio clip: {}",
            m_AudioClips.LostSoulSpawn->GetName());

        m_LostSoulId = *entityIdResult;

        EnterWaitingForDropoff(game);
    }
}

void
Tombstone::TickWaitingForDropoff(const float /*deltaTime*/, Game& /*game*/)
{
}

void
Tombstone::TickDormant(const float /*deltaTime*/, Game& /*game*/)
{
}
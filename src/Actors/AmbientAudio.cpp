#include "AmbientAudio.h"

#include "Game.h"

namespace
{

constexpr const std::string_view kAudioClipName_Crickets{ "Crickets" };
constexpr const std::string_view kAudioClipName_Crow1{ "Crow1" };
constexpr const std::string_view kAudioClipName_Crow2{ "Crow2" };
constexpr const std::string_view kAudioClipName_Owl1{ "Owl1" };
constexpr const std::string_view kAudioClipName_Owl2{ "Owl2" };
constexpr const std::string_view kAudioClipName_Owl3{ "Owl3" };
constexpr const std::string_view kAudioClipName_Wolf1{ "Wolf1" };
constexpr const std::string_view kAudioClipName_Wolf2{ "Wolf2" };
constexpr const std::string_view kAudioClipName_Wolf3{ "Wolf3" };
constexpr const std::string_view kAudioClipName_Wolf4{ "Wolf4" };
constexpr const std::string_view kAudioClipName_Peacock1{ "Peacock1" };
constexpr const std::string_view kAudioClipName_Peacock2{ "Peacock2" };
constexpr const std::string_view kAudioClipName_Dog1{ "Dog1" };

constexpr float kCrowMinInterval{ 10.0f };
constexpr float kCrowMaxInterval{ 15.0f };
constexpr float kOwlMinInterval{ 15.0f };
constexpr float kOwlMaxInterval{ 20.0f };
constexpr float kWolfMinInterval{ 20.0f };
constexpr float kWolfMaxInterval{ 25.0f };
constexpr float kPeacockMinInterval{ 20.0f };
constexpr float kPeacockMaxInterval{ 40.0f };
constexpr float kDogMinInterval{ 20.0f };
constexpr float kDogMaxInterval{ 25.0f };

constexpr AudioPlayer::PlayParams kAudioPlayParams //
    {
        .Gain = 0.25f,
    };

} // namespace

AmbientAudio::AmbientAudio(const AudioClips& audioClips)
    : m_AudioClips(audioClips)
{
}

Result<AmbientAudio>
AmbientAudio::Create(Game& game)
{
    const Level& level = game.GetLevel();

    constexpr std::string_view clipNames[] //
        {
            kAudioClipName_Crickets,
            kAudioClipName_Crow1,
            kAudioClipName_Crow2,
            kAudioClipName_Owl1,
            kAudioClipName_Owl2,
            kAudioClipName_Owl3,
            kAudioClipName_Wolf1,
            kAudioClipName_Wolf2,
            kAudioClipName_Wolf3,
            kAudioClipName_Wolf4,
            kAudioClipName_Peacock1,
            kAudioClipName_Peacock2,
            kAudioClipName_Dog1,
        };

    const AudioClip* clips[std::size(clipNames)];

    for(size_t i = 0; i < std::size(clipNames); ++i)
    {
        const std::string_view clipName = clipNames[i];
        GG_TRY(clip, level.GetAudioClip(clipName), "Failed to load audio clip: {}", clipName);
        clips[i] = &clip;
    }

    GG_TRY(clipCrickets,
        level.GetAudioClip(kAudioClipName_Crickets),
        "Failed to load audio clip: {}",
        kAudioClipName_Crickets);

    GG_VERIFYB(game.GetAudioPlayer().Loop(clipCrickets),
        "Failed to play audio clip: {}",
        clipCrickets.GetName());

    const AudioClips audioClips //
        {
            .Crickets = clips[0],
            .Crow = { clips[1], clips[2] },
            .Owl = { clips[3], clips[4], clips[5] },
            .Wolf = { clips[6], clips[7], clips[8], clips[9] },
            .Peacock = { clips[10], clips[11] },
            .Dog = { clips[12] },
        };

    AmbientAudio ambientAudio{ audioClips };

    const float crowInterval = game.GetEntityRng().NextInRange(kCrowMinInterval, kCrowMaxInterval);
    const float owlInterval = game.GetEntityRng().NextInRange(kOwlMinInterval, kOwlMaxInterval);
    const float wolfInterval = game.GetEntityRng().NextInRange(kWolfMinInterval, kWolfMaxInterval);
    const float peacockInterval = game.GetEntityRng().NextInRange(kPeacockMinInterval, kPeacockMaxInterval);
    const float dogInterval = game.GetEntityRng().NextInRange(kDogMinInterval, kDogMaxInterval);

    ambientAudio.m_CrowTimer.Start(crowInterval);
    ambientAudio.m_OwlTimer.Start(owlInterval);
    ambientAudio.m_WolfTimer.Start(wolfInterval);
    ambientAudio.m_PeacockTimer.Start(peacockInterval);
    ambientAudio.m_DogTimer.Start(dogInterval);

    return ambientAudio;
}

Result<>
AmbientAudio::Tick(const float deltaTime, Game& game)
{
    m_CrowTimer.Tick(deltaTime);
    m_OwlTimer.Tick(deltaTime);
    m_WolfTimer.Tick(deltaTime);
    m_PeacockTimer.Tick(deltaTime);
    m_DogTimer.Tick(deltaTime);

    if(m_CrowTimer.IsExpired())
    {
        const uint32_t count = static_cast<uint32_t>(std::size(m_AudioClips.Crow));

        const uint32_t index = game.GetEntityRng().NextBelow(count);
        const AudioClip* clip = m_AudioClips.Crow[index];

        GG_VERIFYB(game.GetAudioPlayer().Play(*clip, kAudioPlayParams),
            "Failed to play audio clip: {}",
            clip->GetName());

        const float interval = game.GetEntityRng().NextInRange(kCrowMinInterval, kCrowMaxInterval);
        m_CrowTimer.Start(interval);
    }

    if(m_OwlTimer.IsExpired())
    {
        const uint32_t count = static_cast<uint32_t>(std::size(m_AudioClips.Owl));

        const uint32_t index = game.GetEntityRng().NextBelow(count);
        const AudioClip* clip = m_AudioClips.Owl[index];

        GG_VERIFYB(game.GetAudioPlayer().Play(*clip, kAudioPlayParams),
            "Failed to play audio clip: {}",
            clip->GetName());

        const float interval = game.GetEntityRng().NextInRange(kOwlMinInterval, kOwlMaxInterval);
        m_OwlTimer.Start(interval);
    }

    if(m_WolfTimer.IsExpired())
    {
        const uint32_t count = static_cast<uint32_t>(std::size(m_AudioClips.Wolf));

        const uint32_t index = game.GetEntityRng().NextBelow(count);
        const AudioClip* clip = m_AudioClips.Wolf[index];

        GG_VERIFYB(game.GetAudioPlayer().Play(*clip, kAudioPlayParams),
            "Failed to play audio clip: {}",
            clip->GetName());

        const float interval = game.GetEntityRng().NextInRange(kWolfMinInterval, kWolfMaxInterval);
        m_WolfTimer.Start(interval);
    }

    if(m_PeacockTimer.IsExpired())
    {
        const uint32_t count = static_cast<uint32_t>(std::size(m_AudioClips.Peacock));

        const uint32_t index = game.GetEntityRng().NextBelow(count);
        const AudioClip* clip = m_AudioClips.Peacock[index];

        GG_VERIFYB(game.GetAudioPlayer().Play(*clip, kAudioPlayParams),
            "Failed to play audio clip: {}",
            clip->GetName());

        const float interval = game.GetEntityRng().NextInRange(kPeacockMinInterval, kPeacockMaxInterval);
        m_PeacockTimer.Start(interval);
    }

    if(m_DogTimer.IsExpired())
    {
        const uint32_t count = static_cast<uint32_t>(std::size(m_AudioClips.Dog));

        const uint32_t index = game.GetEntityRng().NextBelow(count);
        const AudioClip* clip = m_AudioClips.Dog[index];

        GG_VERIFYB(game.GetAudioPlayer().Play(*clip, kAudioPlayParams),
            "Failed to play audio clip: {}",
            clip->GetName());

        const float interval = game.GetEntityRng().NextInRange(kDogMinInterval, kDogMaxInterval);
        m_DogTimer.Start(interval);
    }

    return Result<>::Ok;
}
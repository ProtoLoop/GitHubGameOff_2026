#pragma once

#include "EntityId.h"
#include "Sprite.h"
#include "Transform2D.h"

#include <glm/vec2.hpp>

class Game;
class Prop;
class LostSoul;
class Renderer;
class AudioClip;

class Caretaker : public Transform2DLike<Caretaker>
{
public:
    Caretaker() = delete;
    ~Caretaker() = default;
    Caretaker(const Caretaker&) = delete;
    Caretaker& operator=(const Caretaker&) = delete;
    Caretaker(Caretaker&&) = default;
    Caretaker& operator=(Caretaker&&) = default;

    static Result<Caretaker> Create(Game& game);

    EntityId GetEntityId() const { return m_Id; }

    SpriteFrame GetSpriteFrame() const { return m_CurSprite.GetFrame(); }

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer) const;

    Rectf GetCollisionBounds() const { return GetSpriteFrame().GetCollisionBounds(); }

    std::span<const Rectf> GetCollisionRects() const
    {
        return GetSpriteFrame().GetCollisionRects();
    }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    void OnCollided(Game& game,
        const EntityId entityId,
        const glm::vec2& collisionNormal,
        const float penDepth);

private:
    struct Sprites
    {
        Sprite::Instance Idle;
        Sprite::Instance Run;
    };

    struct AudioClips
    {
        const AudioClip* LostSoulCollect;
        const AudioClip* LostSoulDropoff;
    };

    Caretaker(const Sprites& sprites, const AudioClips& audioClips);

    EntityId m_Id{ EntityId::NextId() };

    Transform2D m_Transform;
    glm::vec2 m_Velocity{};

    Sprites m_Sprites;
    AudioClips m_AudioClips;
    Sprite::Instance m_CurSprite;
    float m_CurSpritePeriod;

    // The ID of the lost soul currently being carried by the caretaker, if any.
    std::optional<EntityId> m_LostSoulId;
    // The ID of the tombstone associated with the lost soul being carried, if any.
    std::optional<EntityId> m_TombstoneId;
};
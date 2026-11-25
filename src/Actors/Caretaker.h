#pragma once

#include "Sprite.h"
#include "Transform2D.h"
#include <glm/vec2.hpp>

class Game;
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

    SpriteFrame GetSpriteFrame() const { return m_CurSprite.GetFrame(); }

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer) const;

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

private:

    struct Sprites
    {
        Sprite::Instance Idle;
        Sprite::Instance Run;
    };

    struct AudioClips
    {
        const AudioClip* Ding;
    };

    Caretaker(const Sprites& sprites, const AudioClips& audioClips);

    Transform2D m_Transform;
    glm::vec2 m_Velocity{};

    Sprites m_Sprites;
    [[maybe_unused]]AudioClips m_AudioClips;
    Sprite::Instance m_CurSprite;
    float m_CurSpritePeriod;
};
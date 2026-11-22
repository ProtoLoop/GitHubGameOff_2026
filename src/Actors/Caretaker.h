#pragma once

#include "Sprite.h"
#include "Transform2D.h"
#include <glm/vec2.hpp>

class Game;
class Renderer;

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

    SpriteFrame GetSpriteFrame() const { return m_CurAnim.GetFrame(); }

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer) const;

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

private:

    struct Anims
    {
        SpriteAnim::Instance Idle;
        SpriteAnim::Instance Run;
    };

    explicit Caretaker(const Anims& anims);

    Transform2D m_Transform;
    glm::vec2 m_Velocity{};

    Anims m_Anims;
    SpriteAnim::Instance m_CurAnim;
    float m_CurAnimPeriod;
};
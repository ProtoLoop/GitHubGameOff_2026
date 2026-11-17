#pragma once

#include "Sprite.h"
#include "Transform2D.h"

class Game;
class Input;

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

    Result<> Tick(const float deltaTime, Game& game);

    Result<> Render(Renderer& renderer);

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

private:

    explicit Caretaker(const SpriteAnim::Instance& initialAnim);

    Transform2D m_Transform;

    SpriteAnim::Instance m_CurAnim;
};
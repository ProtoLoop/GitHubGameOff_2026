#pragma once

#include "Sprite.h"
#include "Transform2D.h"

class Level;
class Input;
class SpriteAnimNode;

class Caretaker : public Transform2DLike<Caretaker>
{
public:
    Caretaker() = delete;
    ~Caretaker() = default;
    Caretaker(const Caretaker&) = delete;
    Caretaker& operator=(const Caretaker&) = delete;
    Caretaker(Caretaker&&) = default;
    Caretaker& operator=(Caretaker&&) = default;

    static Result<Caretaker> Create(Level& level);

    Result<> Tick(const float deltaTime, const Input& input, const Level& level);

    Result<> Render(Renderer& renderer);

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

private:
    friend class Game;

    explicit Caretaker(const SpriteAnim::Instance& initialAnim);

    Transform2D m_Transform;

    SpriteAnim::Instance m_CurAnim;
};
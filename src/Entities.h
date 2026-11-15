#pragma once

#include "Nodes.h"

class Level;
class Input;
class SpriteAnimNode;

class Caretaker : public Node2DLike<Caretaker>
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

private:

    friend class Node2DLike<Caretaker>;

    explicit Caretaker(SpriteAnimNode& animNode)
        : m_AnimNode(&animNode)
    {
    }

    Node2D& GetNode() { return m_AnimNode->GetNode(); }
    const Node2D& GetNode() const { return m_AnimNode->GetNode(); }

    SpriteAnimNode* m_AnimNode{ nullptr };
};
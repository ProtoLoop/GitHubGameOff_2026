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

    Result<> Tick(const float deltaTime, const Input& input, const Level& level);

    BorrowedHandle<Node2D> GetNode() { return m_Node2D.Borrow(); }

    BorrowedHandle<const Node2D> GetNode() const { return m_Node2D.Borrow(); }

private:
    friend class Game;

    static Result<Caretaker> Create(Level& level);

    Caretaker(OwnedNodeHandle<Node2D>&& node2D, OwnedNodeHandle<SpriteAnimNode>&& animNode);

    OwnedNodeHandle<Node2D> m_Node2D;
    OwnedNodeHandle<SpriteAnimNode> m_AnimNode;
};
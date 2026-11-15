#pragma once

#include "Nodes.h"

class Level;
class Input;
class SpriteAnimNode;

class Caretaker : public Node2DLike<Caretaker>
{
public:
    Caretaker() = delete;
    ~Caretaker();
    Caretaker(const Caretaker&) = delete;
    Caretaker& operator=(const Caretaker&) = delete;
    Caretaker(Caretaker&&) = default;
    Caretaker& operator=(Caretaker&&) = default;

    Result<> Tick(const float deltaTime, const Input& input, const Level& level);

    BorrowedNodeHandle<Node2D> GetNode() { return m_Node2D.Borrow(); }

    BorrowedNodeHandle<const Node2D> GetNode() const { return m_Node2D.Borrow(); }

private:
    friend class Game;

    static Result<Caretaker> Create(Level& level);

    Caretaker(OwnedNodeHandle<Node2D>&& node2D, OwnedNodeHandle<SpriteAnimNode>&& animNode);

    void Destroy(Level& level);

    OwnedNodeHandle<Node2D> m_Node2D;
    OwnedNodeHandle<SpriteAnimNode> m_AnimNode;
};
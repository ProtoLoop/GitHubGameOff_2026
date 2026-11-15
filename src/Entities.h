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

    NodeHandle<Node2D> GetNode() { return m_Node2D; }

    NodeHandle<const Node2D> GetNode() const { return m_Node2D; }

private:
    explicit Caretaker(NodeHandle<Node2D> node2D, NodeHandle<SpriteAnimNode> animNode);

    NodeHandle<Node2D> m_Node2D;
    NodeHandle<SpriteAnimNode> m_AnimNode;
};
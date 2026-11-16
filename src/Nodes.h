#pragma once

#include "Angle.h"
#include "Handle.h"
#include "Sprite.h"

#include <glm/ext/vector_float2.hpp>
#include <glm/vec2.hpp>
#include <string_view>

class Level;

template<typename T>
using OwnedNodeHandle = OwnedHandle<T, Level>;

/// Represents a 2D transform with position and rotation.
/// Y-up coordinates with compass bearings.
/// North: 0 degrees, +Y
/// East: 90 degrees, +X
/// South: 180 degrees, -Y
/// West: 270 degrees, -X
class Transform2D
{
public:
    /// Returns the forward direction vector from a given rotation angle.
    static glm::vec2 ForwardFromRotation(const Angle& rot)
    {
        return glm::vec2(rot.Sin(), rot.Cos());
    }

    /// Returns the right direction vector from a given rotation angle.
    static glm::vec2 RightFromRotation(const Angle& rot)
    {
        return glm::vec2(rot.Cos(), -rot.Sin());
    }

    /// Returns the forward direction vector of this transform.
    glm::vec2 GetForward() const { return ForwardFromRotation(Rotation); }

    /// Returns the right direction vector of this transform.
    glm::vec2 GetRight() const { return RightFromRotation(Rotation); }

    /// The position of this transform.
    glm::vec2 Position{};

    /// The rotation of this transform.
    Angle Rotation;
};

class Node2D
{
public:
    Node2D() = default;
    ~Node2D() = default;
    Node2D(const Node2D&) = delete;
    Node2D& operator=(const Node2D&) = delete;

    /// Moves the node to the specified position (x, y).
    Node2D& MoveTo(const float x, const float y)
    {
        m_Transform.Position = glm::vec2(x, y);
        return *this;
    }

    /// Moves the node to the specified position vector.
    Node2D& MoveTo(const glm::vec2& pos)
    {
        m_Transform.Position = pos;
        return *this;
    }

    /// Moves the node by the specified delta vector.
    Node2D& MoveBy(const glm::vec2& delta)
    {
        m_Transform.Position += delta;
        return *this;
    }

    /// Rotates the node to the specified rotation angle.
    Node2D& RotateTo(const Angle& rot)
    {
        m_Transform.Rotation = rot;
        return *this;
    }

    /// Rotates the node by the specified delta angle.
    Node2D& RotateBy(const Angle& delta)
    {
        m_Transform.Rotation += delta;
        return *this;
    }

    /// Returns the transform of this node.
    const Transform2D& GetTransform() const { return m_Transform; }

    /// Returns the position of this node.
    const glm::vec2& GetPos() const { return m_Transform.Position; }

    /// Returns the rotation of this node.
    const Angle& GetRot() const { return m_Transform.Rotation; }

    /// Returns the forward direction vector of this node.
    glm::vec2 GetForward() const { return m_Transform.GetForward(); }

    /// Returns the right direction vector of this node.
    glm::vec2 GetRight() const { return m_Transform.GetRight(); }

private:
    friend class Level;

    // Only Level can move.
    Node2D(Node2D&&) = default;
    Node2D& operator=(Node2D&&) = default;

    Transform2D m_Transform;
};

template<typename T>
class Node2DLike
{
public:
    /// Moves the node to the specified position (x, y).
    T& MoveTo(const float x, const float y)
    {
        static_cast<T*>(this)->GetNode()->MoveTo(x, y);
        return static_cast<T&>(*this);
    }

    /// Moves the node to the specified position vector.
    T& MoveTo(const glm::vec2& pos)
    {
        static_cast<T*>(this)->GetNode()->MoveTo(pos);
        return static_cast<T&>(*this);
    }

    /// Moves the node by the specified delta vector.
    T& MoveBy(const glm::vec2& delta)
    {
        static_cast<T*>(this)->GetNode()->MoveBy(delta);
        return static_cast<T&>(*this);
    }

    /// Rotates the node to the specified rotation angle.
    T& RotateTo(const Angle& rot)
    {
        static_cast<T*>(this)->GetNode()->RotateTo(rot);
        return static_cast<T&>(*this);
    }

    /// Rotates the node by the specified delta angle.
    T& RotateBy(const Angle& delta)
    {
        static_cast<T*>(this)->GetNode()->RotateBy(delta);
        return static_cast<T&>(*this);
    }

    /// Returns the transform of this node.
    const Transform2D& GetTransform() const
    {
        return static_cast<const T*>(this)->GetNode()->GetTransform();
    }

    /// Returns the position of this node.
    const glm::vec2& GetPos() const { return static_cast<const T*>(this)->GetNode()->GetPos(); }

    /// Returns the rotation of this node.
    const Angle& GetRot() const { return static_cast<const T*>(this)->GetNode()->GetRot(); }

    /// Returns the forward direction vector of this node.
    glm::vec2 GetForward() const { return static_cast<const T*>(this)->GetNode()->GetForward(); }

    /// Returns the right direction vector of this node.
    glm::vec2 GetRight() const { return static_cast<const T*>(this)->GetNode()->GetRight(); }

private:
    friend T;

    Node2DLike() = default;
};

/// Represents an animated sprite node.
/// The node's position defines where it is drawn in the world.
/// The node's rotation can be used by game code to set the facing direction of the sprite.
/// Rotation is NOT used for drawing; sprite images will not be rotated based
/// on the node's rotation.
class SpriteAnimNode : public Node2DLike<SpriteAnimNode>
{
public:
    SpriteAnimNode() = delete;
    ~SpriteAnimNode() = default;
    SpriteAnimNode(const SpriteAnimNode&) = delete;
    SpriteAnimNode& operator=(const SpriteAnimNode&) = delete;
    
    SpriteAnimNode(BorrowedHandle<Node2D> node, const SpriteAnim::Instance& instance)
        : m_Node(node),
          m_CurAnim(instance)
    {
    }

    void Play(const SpriteAnim::Instance& instance);

    std::string_view WhatsPlaying() const;

    SpriteAnimNode& FaceDirection(const Angle& facing);

    Angle GetFaceDirection() const;

    /// Sets the period of the current animation instance.
    void SetPeriod(const float period);

    const SpriteAnim::Instance& GetCurrentAnim() const;

    const Sprite& GetCurrentSprite() const;

    void Tick(const float deltaTime);

    BorrowedHandle<Node2D> GetNode();

    BorrowedHandle<const Node2D> GetNode() const;

private:
    friend class Level;

    // Only Level can move.
    SpriteAnimNode(SpriteAnimNode&&) = default;
    SpriteAnimNode& operator=(SpriteAnimNode&&) = default;

    BorrowedHandle<Node2D> m_Node;
    SpriteAnim::Instance m_CurAnim;
};

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
    glm::vec2 GetForward() const { return ForwardFromRotation(Rot); }

    /// Returns the right direction vector of this transform.
    glm::vec2 GetRight() const { return RightFromRotation(Rot); }

    friend Transform2D operator*(const Transform2D& lhs, const Transform2D& rhs)
    {
        Transform2D result;
        result.Pos = lhs.Pos + (lhs.GetRight() * rhs.Pos.x + lhs.GetForward() * rhs.Pos.y);
        result.Rot = lhs.Rot + rhs.Rot;
        return result;
    }

    /// The position of this transform.
    glm::vec2 Pos{};

    /// The rotation of this transform.
    Angle Rot;
};

/// Stores a transform relative to an optional parent.
/// Position, rotation, and direction getters return values local
/// to the parent's coordinate space.
/// Movement and rotation methods also operate in local space.
/// Without a parent local space is the same as world space.
/// The parent is borrowed and must outlive this node.
class Node2D
{
public:
    struct PassKey
    {
    private:
        friend class Level;
        PassKey() = default;
    };

    explicit Node2D(const BorrowedHandle<Node2D>& parent, const PassKey&)
        : m_Parent(parent)
    {
    }

    Node2D() = default;
    ~Node2D() = default;
    Node2D(const Node2D&) = delete;
    Node2D& operator=(const Node2D&) = delete;

    /// Moves the node to the specified position (x, y).
    Node2D& MoveTo(const float x, const float y)
    {
        m_Transform.Pos = glm::vec2(x, y);
        return *this;
    }

    /// Moves the node to the specified position vector.
    Node2D& MoveTo(const glm::vec2& pos)
    {
        m_Transform.Pos = pos;
        return *this;
    }

    /// Moves the node by the specified delta vector.
    Node2D& MoveBy(const glm::vec2& delta)
    {
        m_Transform.Pos += delta;
        return *this;
    }

    /// Rotates the node to the specified rotation angle.
    Node2D& RotateTo(const Angle& rot)
    {
        m_Transform.Rot = rot;
        return *this;
    }

    /// Rotates the node by the specified delta angle.
    Node2D& RotateBy(const Angle& delta)
    {
        m_Transform.Rot += delta;
        return *this;
    }

    /// Returns the transform of this node.
    const Transform2D& GetTransform() const { return m_Transform; }

    /// Returns the position of this node.
    const glm::vec2& GetPos() const { return m_Transform.Pos; }

    /// Returns the rotation of this node.
    const Angle& GetRot() const { return m_Transform.Rot; }

    /// Returns the forward direction vector of this node.
    glm::vec2 GetForward() const { return m_Transform.GetForward(); }

    /// Returns the right direction vector of this node.
    glm::vec2 GetRight() const { return m_Transform.GetRight(); }

    /// Composes ancestor transforms with this node's local transform to return world space.
    Transform2D GetWorldTransform() const
    {
        // FIXME (KB) - optimize by storing transforms in dense arrays in depth first order.
        if (m_Parent)
        {
            return m_Parent->GetWorldTransform() * m_Transform;
        }
        return m_Transform;
    }

private:
    friend class Level;

    // Only Level can move.
    Node2D(Node2D&&) = default;
    Node2D& operator=(Node2D&&) = default;

    Transform2D m_Transform;
    BorrowedHandle<Node2D> m_Parent;
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
/// Owns a separate Node2D and releases it when destroyed.
/// The node's position defines where it is drawn in the world.
/// The node's rotation can be used by game code to set the facing direction of the sprite.
/// Rotation is NOT used for drawing; sprite images will not be rotated based
/// on the node's rotation.
class SpriteAnimNode : public Node2DLike<SpriteAnimNode>
{
public:
    SpriteAnimNode() = delete;
    ~SpriteAnimNode();
    SpriteAnimNode(const SpriteAnimNode&) = delete;
    SpriteAnimNode& operator=(const SpriteAnimNode&) = delete;
    
    SpriteAnimNode(OwnedNodeHandle<Node2D>&& node, const SpriteAnim::Instance& instance);

    void Play(const SpriteAnim::Instance& instance);

    std::string_view WhatsPlaying() const;

    SpriteAnimNode& FaceDirection(const Angle& facing);

    Angle GetFaceDirection() const;

    /// Sets the period of the current animation instance.
    void SetPeriod(const float period);

    const SpriteAnim::Instance& GetCurrentAnim() const;

    Sprite GetCurrentSprite() const;

    void Tick(const float deltaTime);

    BorrowedHandle<Node2D> GetNode();

    BorrowedHandle<const Node2D> GetNode() const;

private:
    friend class Level;

    // Only Level can move.
    SpriteAnimNode(SpriteAnimNode&&) noexcept;
    SpriteAnimNode& operator=(SpriteAnimNode&&) noexcept;

    OwnedNodeHandle<Node2D> m_Node;
    SpriteAnim::Instance m_CurAnim;
};

#pragma once

#include "Angle.h"

#include <glm/ext/vector_float2.hpp>
#include <glm/vec2.hpp>

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

    /// Moves the node to the specified position (x, y).
    Transform2D& MoveTo(const float x, const float y)
    {
        Pos = glm::vec2(x, y);
        return *this;
    }

    /// Moves the node to the specified position vector.
    Transform2D& MoveTo(const glm::vec2& pos)
    {
        Pos = pos;
        return *this;
    }

    /// Moves the node by the specified delta vector.
    Transform2D& MoveBy(const glm::vec2& delta)
    {
        Pos += delta;
        return *this;
    }

    /// Rotates the node to the specified rotation angle.
    Transform2D& RotateTo(const Angle& rot)
    {
        Rot = rot;
        return *this;
    }

    /// Rotates the node by the specified delta angle.
    Transform2D& RotateBy(const Angle& delta)
    {
        Rot += delta;
        return *this;
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

template<typename T>
class Transform2DLike
{
public:
    /// Moves the node to the specified position (x, y).
    T& MoveTo(const float x, const float y)
    {
        static_cast<T*>(this)->GetTransform().MoveTo(x, y);
        return static_cast<T&>(*this);
    }

    /// Moves the node to the specified position vector.
    T& MoveTo(const glm::vec2& pos)
    {
        static_cast<T*>(this)->GetTransform().MoveTo(pos);
        return static_cast<T&>(*this);
    }

    /// Moves the node by the specified delta vector.
    T& MoveBy(const glm::vec2& delta)
    {
        static_cast<T*>(this)->GetTransform().MoveBy(delta);
        return static_cast<T&>(*this);
    }

    /// Rotates the node to the specified rotation angle.
    T& RotateTo(const Angle& rot)
    {
        static_cast<T*>(this)->GetTransform().RotateTo(rot);
        return static_cast<T&>(*this);
    }

    /// Rotates the node by the specified delta angle.
    T& RotateBy(const Angle& delta)
    {
        static_cast<T*>(this)->GetTransform().RotateBy(delta);
        return static_cast<T&>(*this);
    }

    /// Returns the position of this node.
    const glm::vec2& GetPos() const { return static_cast<const T*>(this)->GetTransform().Pos; }

    /// Returns the rotation of this node.
    const Angle& GetRot() const { return static_cast<const T*>(this)->GetTransform().Rot; }

    /// Returns the forward direction vector of this node.
    glm::vec2 GetForward() const { return static_cast<const T*>(this)->GetTransform().GetForward(); }

    /// Returns the right direction vector of this node.
    glm::vec2 GetRight() const { return static_cast<const T*>(this)->GetTransform().GetRight(); }

private:
    friend T;

    Transform2DLike() = default;
};
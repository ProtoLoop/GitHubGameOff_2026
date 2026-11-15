#pragma once

#include "Angle.h"
#include "Sprite.h"

#include <glm/ext/vector_float2.hpp>
#include <glm/vec2.hpp>
#include <string_view>

class NodeHandleSlot
{
public:

    void* m_Ptr{ nullptr };
    uint32_t m_Generation{ 0 };
    bool m_Allocated{ false };
};

template<typename T>
class OwnedNodeHandle;

/// A handle that provides temporary, non-owning access to a node of type T.
template<typename T>
class BorrowedNodeHandle
{
public:
    BorrowedNodeHandle() = delete;
    ~BorrowedNodeHandle() = default;
    BorrowedNodeHandle(const BorrowedNodeHandle&) = default;
    BorrowedNodeHandle& operator=(const BorrowedNodeHandle&) = default;
    BorrowedNodeHandle(BorrowedNodeHandle&&) = default;
    BorrowedNodeHandle& operator=(BorrowedNodeHandle&&) = default;

    T* operator->()
    {
        GG_ASSERT(IsValid());
        return static_cast<T*>(m_Slot->m_Ptr);
    }

    const T* operator->() const
    {
        GG_ASSERT(IsValid());
        return static_cast<const T*>(m_Slot->m_Ptr);
    }

    bool IsValid() const
    {
        return m_Slot && m_Slot->m_Generation == m_Generation;
    }

    explicit operator bool() const { return IsValid(); }

    template<typename>
    friend class BorrowedNodeHandle;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator BorrowedNodeHandle<const T>() const
        requires(!std::is_const_v<T>)
    {
        return BorrowedNodeHandle<const T>(*m_Slot, m_Generation);
    }

    friend auto operator<=>(const BorrowedNodeHandle& lhs, const BorrowedNodeHandle& rhs)
    {
        if(lhs.m_Slot != rhs.m_Slot)
        {
            return lhs.m_Slot->m_Ptr <=> rhs.m_Slot->m_Ptr;
        }
        return lhs.m_Generation <=> rhs.m_Generation;
    }

private:
    friend OwnedNodeHandle<T>;

    BorrowedNodeHandle(NodeHandleSlot& slot, uint32_t generation)
        : m_Slot(&slot),
          m_Generation(generation)
    {
    }

    NodeHandleSlot* m_Slot;
    uint32_t m_Generation;
};

/// A handle that owns a node of type T and manages its lifetime.
/// The handle must be explicitly destroyed or moved before the destructor is called.
template<typename T>
class OwnedNodeHandle
{
public:
    OwnedNodeHandle() = delete;
    ~OwnedNodeHandle()
    {
        GG_ASSERT(!m_Slot || (!m_Slot->m_Allocated && !m_Slot->m_Ptr),
            "Attempt to destroy an active OwnedNodeHandle - this will leak a slot, a node, or both");
    }
    OwnedNodeHandle(const OwnedNodeHandle&) = delete;
    OwnedNodeHandle& operator=(const OwnedNodeHandle&) = delete;
    OwnedNodeHandle(OwnedNodeHandle&& other) noexcept
        : m_Slot(other.m_Slot),
          m_Generation(other.m_Generation)
    {
        other.m_Slot = nullptr;
        other.m_Generation = 0;
    }

    BorrowedNodeHandle<T> Borrow() const
    {
        GG_ASSERT(IsValid());
        return BorrowedNodeHandle<T>(*m_Slot, m_Generation);
    }

    OwnedNodeHandle& operator=(OwnedNodeHandle&&other) noexcept
    {
        if(this != &other)
        {
            GG_ASSERT(!m_Slot || (!m_Slot->m_Allocated && !m_Slot->m_Ptr),
                "Attempt to move to an active OwnedNodeHandle - this will leak a slot, a node, or both");

            m_Slot = other.m_Slot;
            m_Generation = other.m_Generation;
            other.m_Slot = nullptr;
            other.m_Generation = 0;
        }
        return *this;
    }

    T* operator->()
    {
        GG_ASSERT(IsValid());
        return static_cast<T*>(m_Slot->m_Ptr);
    }

    const T* operator->() const
    {
        GG_ASSERT(IsValid());
        return static_cast<const T*>(m_Slot->m_Ptr);
    }

    bool IsValid() const
    {
        return m_Slot && m_Slot->m_Generation == m_Generation;
    }

    explicit operator bool() const { return IsValid(); }

    friend auto operator<=>(const OwnedNodeHandle& lhs, const OwnedNodeHandle& rhs)
    {
        if(lhs.m_Slot != rhs.m_Slot)
        {
            if(!lhs.m_Slot)
            {
                return std::strong_ordering::less;
            }
            if(!rhs.m_Slot)
            {
                return std::strong_ordering::greater;
            }
            return lhs.m_Slot->m_Ptr <=> rhs.m_Slot->m_Ptr;
        }
        return lhs.m_Generation <=> rhs.m_Generation;
    }

private:
    friend class Level;

    explicit OwnedNodeHandle(NodeHandleSlot& slot)
        : m_Slot(&slot),
          m_Generation(slot.m_Generation)
    {
    }

    OwnedNodeHandle(NodeHandleSlot& slot, uint32_t generation)
        : m_Slot(&slot),
          m_Generation(generation)
    {
    }

    NodeHandleSlot* m_Slot;
    uint32_t m_Generation;
};

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
    
    SpriteAnimNode(BorrowedNodeHandle<Node2D> node, const SpriteAnim::Instance& instance)
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

    BorrowedNodeHandle<Node2D> GetNode();

    BorrowedNodeHandle<const Node2D> GetNode() const;

private:
    friend class Level;

    // Only Level can move.
    SpriteAnimNode(SpriteAnimNode&&) = default;
    SpriteAnimNode& operator=(SpriteAnimNode&&) = default;

    BorrowedNodeHandle<Node2D> m_Node;
    SpriteAnim::Instance m_CurAnim;
};
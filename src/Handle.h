#pragma once

#include "Diag.h"

#include <cstdint>
#include <compare>
#include <type_traits>

class HandleSlot
{
public:

    void* m_Ptr{ nullptr };
    uint32_t m_Generation{ 0 };
    uint32_t m_BorrowCount{ 0 };
    bool m_Allocated{ false };
};

template<typename T, typename OwnerType>
class OwnedHandle;

/// A handle that provides temporary, non-owning access to a node of type T.
/// Does not extend the lifetime of the node or its owner. Both must outlive its use.
template<typename T>
class BorrowedHandle
{
public:
    ~BorrowedHandle()
    {
        Release();
    }
    BorrowedHandle(const BorrowedHandle& other)
        : m_Slot(other.m_Slot),
          m_Generation(other.m_Generation)
    {
        AddRef();
    }
    BorrowedHandle& operator=(const BorrowedHandle& other)
    {
        if(this != &other)
        {
            Release();

            m_Slot = other.m_Slot;
            m_Generation = other.m_Generation;
            
            AddRef();
        }
        return *this;
    }

    BorrowedHandle(BorrowedHandle&& other) noexcept
        : m_Slot(other.m_Slot),
          m_Generation(other.m_Generation)
    {
        other.m_Slot = nullptr;
        other.m_Generation = 0;
    }

    BorrowedHandle& operator=(BorrowedHandle&& other) noexcept
    {
        if(this != &other)
        {
            Release();

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

    template<typename>
    friend class BorrowedHandle;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator BorrowedHandle<const T>() const
        requires(!std::is_const_v<T>)
    {
        return m_Slot ?  BorrowedHandle<const T>(*m_Slot, m_Generation) : BorrowedHandle<const T>();
    }

    friend auto operator<=>(const BorrowedHandle& lhs, const BorrowedHandle& rhs)
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
    template<typename X, typename Y>
    friend class OwnedHandle;

    BorrowedHandle() = default;

    BorrowedHandle(HandleSlot& slot, uint32_t generation)
        : m_Slot(&slot),
          m_Generation(generation)
    {
        AddRef();
    }

    void AddRef()
    {
        if(m_Slot)
        {
            m_Slot->m_BorrowCount++;
        }
    }

    void Release()
    {
        if(m_Slot)
        {
            GG_ASSERT(m_Slot->m_BorrowCount > 0, "Borrow count is already zero");
            m_Slot->m_BorrowCount--;
        }
    }

    HandleSlot* m_Slot{ nullptr };
    uint32_t m_Generation{ 0 };
};

/// Owns a node and releases it through its owner on destruction.
/// Moving transfers ownership and leaves the source empty. Move assignment first
/// releases the destination's node, unless assigning the handle to itself.
/// The owner must remain at a stable address and outlive the handle.
template<typename T, typename OwnerType>
class OwnedHandle
{
public:

    OwnedHandle() = delete;
    ~OwnedHandle()
    {
        static_assert(
            requires(OwnerType& o, OwnedHandle& handle) { o.Destroy(handle); },
            "OwnerType must have a Destroy() method that accepts an OwnedHandle");

        if(m_Owner)
        {
            m_Owner->Destroy(*this);
        }
    }
    OwnedHandle(const OwnedHandle&) = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;
    OwnedHandle(OwnedHandle&& other) noexcept
        : m_Owner(other.m_Owner),
          m_Slot(other.m_Slot),
          m_Generation(other.m_Generation)
    {
        other.m_Owner = nullptr;
        other.m_Slot = nullptr;
        other.m_Generation = 0;
    }

    BorrowedHandle<T> Borrow() const
    {
        GG_ASSERT(IsValid());
        return BorrowedHandle<T>(*m_Slot, m_Generation);
    }

    OwnedHandle& operator=(OwnedHandle&&other) noexcept
    {
        if(this != &other)
        {
            if(m_Owner)
            {
                m_Owner->Destroy(*this);
            }

            m_Owner = other.m_Owner;
            m_Slot = other.m_Slot;
            m_Generation = other.m_Generation;
            other.m_Owner = nullptr;
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

    friend auto operator<=>(const OwnedHandle& lhs, const OwnedHandle& rhs)
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
    friend OwnerType;

    OwnedHandle(OwnerType& owner, HandleSlot& slot)
        : m_Owner(&owner),
          m_Slot(&slot),
          m_Generation(slot.m_Generation)
    {
    }

    OwnedHandle(OwnerType& owner, HandleSlot& slot, uint32_t generation)
        : m_Owner(&owner),
          m_Slot(&slot),
          m_Generation(generation)
    {
    }

    OwnerType* m_Owner;
    HandleSlot* m_Slot;
    uint32_t m_Generation;
};
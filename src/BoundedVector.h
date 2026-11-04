#pragma once

#include "Result.h"

#include <memory>

/// A std::vector-like container in which capacity is fixed at construction,
/// does not grow, and can contain non-copyable and non-movable types.
template<typename T, typename Allocator = std::allocator<T>>
class BoundedVector
{
public:
    using size_type = size_t;
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    explicit BoundedVector(const size_t capacity) { Reserve(capacity); }

    BoundedVector(const size_t capacity, const Allocator& allocator)
        : m_Allocator(allocator)
    {
        Reserve(capacity);
    }

    ~BoundedVector() { Destroy(); }

    BoundedVector(const BoundedVector&) = delete;

    BoundedVector& operator=(const BoundedVector&) = delete;

    BoundedVector(BoundedVector&& other) noexcept
        : m_Allocator(std::move(other.m_Allocator))
    {
        std::swap(m_Storage, other.m_Storage);
        std::swap(m_Size, other.m_Size);
        std::swap(m_Capacity, other.m_Capacity);
    }

    BoundedVector& operator=(BoundedVector&& other) noexcept
    {
        if(this != &other)
        {
            Destroy();

            m_Allocator = std::move(other.m_Allocator);
            std::swap(m_Storage, other.m_Storage);
            std::swap(m_Size, other.m_Size);
            std::swap(m_Capacity, other.m_Capacity);
        }
        return *this;
    }
    Result<> push_back(const T& value)
    {
        GG_CHECKB(size() < capacity())
        allocator_traits::construct(m_Allocator, AsPointer(m_Size++), value);
        return Result<>::Success();
    }

    Result<> push_back(T&& value)
    {
        GG_CHECKB(size() < capacity())
        allocator_traits::construct(m_Allocator, AsPointer(m_Size++), std::move(value));
        return Result<>::Success();
    }

    template<typename... Args>
    Result<> emplace_back(Args&&... args)
    {
        GG_CHECKB(size() < capacity())
        allocator_traits::construct(m_Allocator, AsPointer(m_Size++), std::forward<Args>(args)...);
        return Result<>::Success();
    }

    T* TryGet(const size_t index) { return index < size() ? AsPointer(index) : nullptr; }

    const T* TryGet(const size_t index) const
    {
        return index < size() ? AsPointer(index) : nullptr;
    }

    constexpr size_t capacity() const { return m_Capacity; }
    constexpr size_t max_size() const { return m_Capacity; }
    size_t size() const { return m_Size; }
    bool empty() const { return size() == 0; }
    bool full() const { return size() == capacity(); }
    void clear()
    {
        for(T& t : *this)
        {
            allocator_traits::destroy(m_Allocator, &t);
        }

        m_Size = 0;
    }

    iterator begin() { return AsPointer(0); }
    iterator end() { return AsPointer(size()); }
    const_iterator begin() const { return AsPointer(0); }
    const_iterator end() const { return AsPointer(size()); }
    const_iterator cbegin() const { return AsPointer(0); }
    const_iterator cend() const { return AsPointer(size()); }

private:
    using allocator_traits = std::allocator_traits<Allocator>;

    void Reserve(size_t capacity)
    {
        GG_ASSERT(!m_Storage);
        m_Storage = allocator_traits::allocate(m_Allocator, capacity);
        m_Capacity = capacity;
    }

    void Destroy()
    {
        clear();
        if(m_Storage)
        {
            allocator_traits::deallocate(m_Allocator, m_Storage, m_Capacity);
            m_Storage = nullptr;
            m_Capacity = 0;
        }
    }

    T* AsPointer(const size_t index)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return &m_Storage[index];
    }
    const T* AsPointer(const size_t index) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return &m_Storage[index];
    }

    Allocator m_Allocator{};

    T* m_Storage{ nullptr };
    size_t m_Size{ 0 };
    size_t m_Capacity{ 0 };
};
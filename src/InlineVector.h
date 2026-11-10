#pragma once

#include "Result.h"

/// A std::vector-like container in which capacity is fixed at compile time,
/// does not grow, and can contain non-copyable and non-movable types.
template<typename T, size_t N>
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class InlineVector
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

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    InlineVector() = default;

    ~InlineVector() { clear(); }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    constexpr InlineVector(const InlineVector& other)
    {
        for(const T& t : other)
        {
            std::construct_at(AsPointer(m_Size++), t);
        }
    }

    template<size_t M>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit constexpr InlineVector(const InlineVector<T, M>& other)
    {
        static_assert(M <= N, "Cannot construct InlineVector from a larger InlineVector");

        for(const T& t : other)
        {
            std::construct_at(AsPointer(m_Size++), t);
        }
    }

    InlineVector& operator=(const InlineVector& other)
    {
        if(this != &other)
        {
            clear();
            for(const T& t : other)
            {
                std::construct_at(AsPointer(m_Size++), t);
            }
        }
        return *this;
    }

    template<size_t M>
    constexpr InlineVector& operator=(const InlineVector<T, M>& other)
    {
        static_assert(M <= N, "Cannot construct InlineVector from a larger InlineVector");

        if constexpr(M == N)
        {
            // Can only check for self assignment when M==N, otherwise the types are different and
            // self assignment is not possible.
            if(this != &other)
            {
                clear();
                for(const T& t : other)
                {
                    std::construct_at(AsPointer(m_Size++), t);
                }
            }
        }
        else
        {
            clear();
            for(const T& t : other)
            {
                std::construct_at(AsPointer(m_Size++), t);
            }
        }
        return *this;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    constexpr InlineVector(InlineVector&& other) noexcept
    {
        for(T& t : other)
        {
            std::construct_at(AsPointer(m_Size++), std::move(t));
        }

        other.clear();
    }

    constexpr InlineVector& operator=(InlineVector&& other) noexcept
    {
        if(this != &other)
        {
            clear();

            for(T& t : other)
            {
                std::construct_at(AsPointer(m_Size++), std::move(t));
            }

            other.clear();
        }
        return *this;
    }

    Result<> push_back(const T& value)
    {
        GG_CHECKB(m_Size < N)
        std::construct_at(AsPointer(m_Size++), value);
        return Result<>::Success();
    }

    Result<> push_back(T&& value)
    {
        GG_CHECKB(m_Size < N)
        std::construct_at(AsPointer(m_Size++), std::move(value));
        return Result<>::Success();
    }

    template<typename... Args>
    Result<> emplace_back(Args&&... args)
    {
        GG_CHECKB(m_Size < N)
        std::construct_at(AsPointer(m_Size++), std::forward<Args>(args)...);
        return Result<>::Success();
    }

    T* TryGet(const size_t index) { return index < size() ? AsPointer(index) : nullptr; }

    const T* TryGet(const size_t index) const
    {
        return index < size() ? AsPointer(index) : nullptr;
    }

    constexpr size_t capacity() const { return N; }
    constexpr size_t max_size() const { return N; }
    size_t size() const { return m_Size; }
    bool empty() const { return m_Size == 0; }
    bool full() const { return m_Size == N; }
    void clear()
    {
        for(T& t : *this)
        {
            std::destroy_at(&t);
        }
        m_Size = 0;
    }

    iterator begin() { return AsPointer(0); }
    iterator end() { return AsPointer(m_Size); }
    const_iterator begin() const { return AsPointer(0); }
    const_iterator end() const { return AsPointer(m_Size); }
    const_iterator cbegin() const { return AsPointer(0); }
    const_iterator cend() const { return AsPointer(m_Size); }

private:
    T* AsPointer(const size_t index)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<T*>(&m_Storage[(sizeof(T) * index)]);
    }
    const T* AsPointer(const size_t index) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const T*>(&m_Storage[(sizeof(T) * index)]);
    }

    alignas(alignof(T)) std::byte m_Storage[sizeof(T) * N];

    size_t m_Size{};
};
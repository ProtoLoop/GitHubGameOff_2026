#pragma once

#include "Diag.h"

#include <expected>
#include <functional>

template<class T = void>
class [[nodiscard]] Result;

template<>
class [[nodiscard]] Result<void>
{
public:
    struct OkType
    {
    };

    struct ErrorType
    {
    };

    static constexpr OkType Ok{};
    static constexpr ErrorType Error{};

    Result() = default;
    ~Result() = default;
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(ErrorType) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(OkType)
        : m_Value(true)
    {
    }

    explicit operator bool() const { return m_Value; }

private:
    bool m_Value{ false };

    explicit Result(const bool value)
        : m_Value(value)
    {
    }
};

template<class T>
class [[nodiscard]] Result<T&>
{
    using Ref = std::reference_wrapper<T>;
    using Storage = std::expected<Ref, Result<>::ErrorType>;

public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    Result() = default;
    ~Result() = default;
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;

    /// Borrows an existing object without extending its lifetime.
    /// Deducing U through U& rejects rvalues, even when T is const.
    /// Requiring U* to convert to T* rejects conversions that create temporaries.
    template<class U>
    // Exclude Result itself from being used as U
        requires(!std::is_same_v<std::remove_cvref_t<U>, Result>)
    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(U& value)
        : m_Value(std::ref(value))
    {
        static_assert(std::convertible_to<U*, T*>,
            "Result<T&> requires a compatible lvalue without temporary conversion");
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(Result<>::ErrorType)
        : m_Value(std::unexpected<Result<>::ErrorType>{ Result<>::Error })
    {
    }

    explicit operator bool() const { return m_Value.has_value(); }
    reference Value() { return m_Value.value(); }

    pointer operator->() { return &m_Value.value().get(); }
    const_pointer operator->() const { return &m_Value.value().get(); }

    reference operator*() { return m_Value.value().get(); }
    const_reference operator*() const { return m_Value.value().get(); }

    void Reset() { m_Value = std::unexpected<Result<>::ErrorType>{ Result<>::Error }; }

private:
    Storage m_Value{ std::unexpected<Result<>::ErrorType>{ Result<>::Error } };

    explicit Result(Storage value)
        : m_Value(std::move(value))
    {
    }
};

template<class T>
class [[nodiscard]] Result
{
    using Storage = std::expected<T, Result<>::ErrorType>;

public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    Result() = default;
    ~Result() = default;
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(T value)
        : m_Value(std::in_place, std::move(value))
    {
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(Result<>::ErrorType)
        : m_Value(std::unexpected<Result<>::ErrorType>{ Result<>::Error })
    {
    }

    explicit operator bool() const { return m_Value.has_value(); }
    reference Value() { return m_Value.value(); }

    pointer operator->() { return &m_Value.value(); }
    const_pointer operator->() const { return &m_Value.value(); }

    reference operator*() { return m_Value.value(); }
    const_reference operator*() const { return m_Value.value(); }

    void Reset() { m_Value = std::unexpected<Result<>::ErrorType>{ Result<>::Error }; }

private:
    Storage m_Value{ std::unexpected<Result<>::ErrorType>{ Result<>::Error } };

    explicit Result(Storage value)
        : m_Value(std::move(value))
    {
    }
};

#define GG_CHECK(expr, ...)                                                                        \
    while(!static_cast<bool>(expr))                                                                \
    {                                                                                              \
        __VA_OPT__(GG_ERROR(__VA_ARGS__));                                                         \
        return Result<>::Error;                                                                    \
    }

#define GG_CHECKB(expr, ...)                                                                       \
    while(!GG_VERIFYB(expr __VA_OPT__(, ) __VA_ARGS__))                                            \
    {                                                                                              \
        return Result<>::Error;                                                                    \
    }

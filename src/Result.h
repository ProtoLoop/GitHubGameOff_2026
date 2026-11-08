#pragma once

#include "Diag.h"

#include <expected>

template<class T = void>
class [[nodiscard]] Result;

template<>
class Result<void>
{
public:
    struct Error
    {
    };

    Result() = default;
    ~Result() = default;
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;

    static Result Success() { return Result{ true }; }

    static Error Fail() { return Error{}; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(Error) {}

    explicit operator bool() const { return m_Value; }

private:
    bool m_Value{ false };

    explicit Result(const bool value)
        : m_Value(value)
    {
    }
};

template<class T>
class Result
{
    using Storage = std::expected<T, Result<>::Error>;

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
    Result(Result<>::Error)
        : m_Value(std::unexpected<Result<>::Error>{ Result<>::Error{} })
    {
    }

    explicit operator bool() const { return m_Value.has_value(); }
    reference Value() { return m_Value.value(); }

    pointer operator->() { return &m_Value.value(); }
    const_pointer operator->() const { return &m_Value.value(); }

    reference operator*() { return m_Value.value(); }
    const_reference operator*() const { return m_Value.value(); }

    void Reset() { m_Value = std::unexpected<Result<>::Error>{Result<>::Error{}}; }

private:
    Storage m_Value{std::unexpected<Result<>::Error>{Result<>::Error{}}};

    explicit Result(Storage value)
        : m_Value(std::move(value))
    {
    }
};

#define GG_CHECK(expr, ...)                                                                        \
    while(!static_cast<bool>(expr))                                                                \
    {                                                                                              \
        __VA_OPT__(GG_ERROR(__VA_ARGS__));                                                         \
        return Result<>::Fail();                                                                   \
    }

#define GG_CHECKB(expr, ...)                                                                       \
    while(!GG_VERIFYB(expr __VA_OPT__(, ) __VA_ARGS__))                                             \
    {                                                                                              \
        return Result<>::Fail();                                                                   \
    }

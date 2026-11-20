#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

struct Rectf
{
    float x;
    float y;
    float w;
    float h;

    constexpr Rectf(const float x_, const float y_, const float w_, const float h_)
        : x(x_),
          y(y_),
          w(w_),
          h(h_)
    {
    }

    constexpr Rectf(const glm::vec2& origin, const glm::vec2& size)
        : x(origin.x),
          y(origin.y),
          w(size.x),
          h(size.y)
    {
    }

    constexpr explicit Rectf(const glm::vec2& size)
        : x(0),
          y(0),
          w(size.x),
          h(size.y)
    {
    }

    constexpr glm::vec2 Origin() const { return glm::vec2(x, y); }

    constexpr glm::vec2 Size() const { return glm::vec2(w, h); }

    constexpr Rectf& operator+=(const glm::vec2& offset)
    {
        x += offset.x;
        y += offset.y;
        return *this;
    }

    constexpr Rectf& operator-=(const glm::vec2& offset)
    {
        x -= offset.x;
        y -= offset.y;
        return *this;
    }

    constexpr Rectf& operator*=(float scale)
    {
        x *= scale;
        y *= scale;
        w *= scale;
        h *= scale;
        return *this;
    }

    constexpr bool Intersects(const Rectf& other) const
    {
        return x + w >= other.x
            && x <= other.x + other.w
            && y + h >= other.y
            && y <= other.y + other.h;
    }

    constexpr bool Contains(const glm::vec2& point) const
    {
        return point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h;
    }

    constexpr friend Rectf operator+(const Rectf& rect, const glm::vec2& offset)
    {
        return Rectf{ rect.x + offset.x, rect.y + offset.y, rect.w, rect.h };
    }

    constexpr friend Rectf operator+(const glm::vec2& offset, const Rectf& rect)
    {
        return Rectf{ rect.x + offset.x, rect.y + offset.y, rect.w, rect.h };
    }

    constexpr friend Rectf operator-(const Rectf& rect, const glm::vec2& offset)
    {
        return Rectf{ rect.x - offset.x, rect.y - offset.y, rect.w, rect.h };
    }

    constexpr friend Rectf operator-(const glm::vec2& offset, const Rectf& rect)
    {
        return Rectf{ offset.x - rect.x, offset.y - rect.y, rect.w, rect.h };
    }

    constexpr friend Rectf operator*(const Rectf& rect, float scale)
    {
        return Rectf{ rect.x * scale, rect.y * scale, rect.w * scale, rect.h * scale };
    }

    constexpr friend Rectf operator*(float scale, const Rectf& rect)
    {
        return Rectf{ rect.x * scale, rect.y * scale, rect.w * scale, rect.h * scale };
    }

    constexpr friend bool operator!=(const Rectf& lhs, const Rectf& rhs) = default;
    constexpr friend bool operator==(const Rectf& lhs, const Rectf& rhs) = default;
};

struct Rect
{
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;

    constexpr Rect(const uint32_t x_, const uint32_t y_, const uint32_t w_, const uint32_t h_)
        : x(x_),
          y(y_),
          w(w_),
          h(h_)
    {
    }

    constexpr Rect(const glm::uvec2& origin, const glm::uvec2& size)
        : x(origin.x),
          y(origin.y),
          w(size.x),
          h(size.y)
    {
    }

    constexpr explicit Rect(const glm::uvec2& size)
        : x(0),
          y(0),
          w(size.x),
          h(size.y)
    {
    }

    constexpr glm::uvec2 Origin() const { return glm::uvec2(x, y); }

    constexpr glm::uvec2 Size() const { return glm::uvec2(w, h); }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator Rectf() const
    {
        return Rectf(static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(w),
            static_cast<float>(h));
    }

    constexpr friend bool operator!=(const Rect& lhs, const Rect& rhs) = default;
    constexpr friend bool operator==(const Rect& lhs, const Rect& rhs) = default;
};

struct Dimension2D
{
    uint32_t w;
    uint32_t h;

    constexpr friend bool operator!=(const Dimension2D& lhs, const Dimension2D& rhs) = default;
    constexpr friend bool operator==(const Dimension2D& lhs, const Dimension2D& rhs) = default;
};
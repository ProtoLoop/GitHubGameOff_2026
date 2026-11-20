#pragma once

#include <cstdint>
#include <glm/ext/vector_float2.hpp>

struct Rectf
{
    float x;
    float y;
    float w;
    float h;

    Rectf operator+(glm::vec2 offset) const
    {
        return Rectf
        {
            .x = x + offset.x,
            .y = y + offset.y,
            .w = w,
            .h = h
        };
    }

    friend bool operator!=(const Rectf& lhs, const Rectf& rhs) = default;
    friend bool operator==(const Rectf& lhs, const Rectf& rhs) = default;
};

struct Dimension2D
{
    uint32_t w;
    uint32_t h;

    friend bool operator!=(const Dimension2D& lhs, const Dimension2D& rhs) = default;
    friend bool operator==(const Dimension2D& lhs, const Dimension2D& rhs) = default;
};

struct Rect
{
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;

    Rectf operator+(glm::vec2 offset) const
    {
        return Rectf
        {
            .x = static_cast<float>(x) + offset.x,
            .y = static_cast<float>(y) + offset.y,
            .w = static_cast<float>(w),
            .h = static_cast<float>(h)
        };
    }

    friend bool operator!=(const Rect& lhs, const Rect& rhs) = default;
    friend bool operator==(const Rect& lhs, const Rect& rhs) = default;
};
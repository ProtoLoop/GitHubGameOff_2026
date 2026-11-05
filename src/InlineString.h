#pragma once

#include "Diag.h"

#include <algorithm>
#include <array>
#include <string_view>

template<std::size_t N>
class InlineString
{
public:
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr InlineString(const std::string_view str)
    {
        if(GG_VERIFY(str.size() < N, "String too long for InlineString"))
        {
            std::ranges::copy(str, std::begin(data));
            m_Size = str.size();
            data[m_Size] = '\0';
        }
        else
        {
            std::ranges::copy(str.substr(0, N - 1), std::begin(data));
            m_Size = N - 1;
            data[m_Size] = '\0';
        }
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator std::string_view() const { return std::string_view(data.data(), m_Size); }

    friend constexpr auto operator<=>(const InlineString& lhs, const InlineString& rhs)
    {
        return std::string_view(lhs) <=> std::string_view(rhs);
    }

    friend constexpr auto operator<=>(const InlineString& lhs, std::string_view rhs)
    {
        return std::string_view(lhs) <=> rhs;
    }

    friend constexpr bool operator==(const InlineString& lhs, const InlineString& rhs)
    {
        return std::string_view(lhs) == std::string_view(rhs);
    }

    friend constexpr bool operator==(const InlineString& lhs, std::string_view rhs)
    {
        return std::string_view(lhs) == rhs;
    }

    constexpr const char* c_str() const { return data.data(); }

    constexpr size_t size() const { return m_Size; }

    constexpr bool empty() const { return m_Size == 0; }

private:
    std::array<char, N> data{};
    size_t m_Size{ 0 };
};

/// Formatter specialization for InlineString to be used with std::format.
template<size_t N>
struct std::formatter<InlineString<N>> : std::formatter<std::string_view>
{
    auto format(const InlineString<N>& str, auto& context) const
    {
        return std::formatter<std::string_view>::format(std::string_view(str.c_str(), str.size()),
            context);
    }
};
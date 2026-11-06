#pragma once

#include <cmath>
// IWYU instructs clangd to keep <compare> for operator<=>, even though it thinks it is unused.
#include <compare> // IWYU pragma: keep
#include <cstdint>

class Angle
{
public:
    Angle() = default;

    explicit Angle(const float turns)
        : m_Turns(MakeTurns(turns))
    {
    }

    float ToTurns() const { return static_cast<float>(m_Turns * kFullTurnScale); }

    /// Get the direction index based on the number of directions.
    uint32_t GetDirection(const uint32_t numDirs) const
    {
        return static_cast<uint32_t>(m_Turns * kFullTurnScale * numDirs) % numDirs;
    }

    Angle& operator+=(const Angle& other)
    {
        m_Turns += other.m_Turns;
        return *this;
    }

    Angle& operator-=(const Angle& other)
    {
        m_Turns -= other.m_Turns;
        return *this;
    }

    friend auto operator<=>(const Angle& lhs, const Angle& rhs)
    {
        return lhs.m_Turns <=> rhs.m_Turns;
    }

    friend bool operator==(const Angle& lhs, const Angle& rhs)
    {
        return lhs.m_Turns == rhs.m_Turns;
    }

    friend Angle operator+(const Angle& lhs, const Angle& rhs)
    {
        return Angle(lhs.m_Turns + rhs.m_Turns);
    }

    friend Angle operator-(const Angle& lhs, const Angle& rhs)
    {
        return Angle(lhs.m_Turns - rhs.m_Turns);
    }

    friend Angle operator/(const Angle& lhs, const float rhs) { return Angle(lhs.ToTurns() / rhs); }

    friend Angle operator*(const Angle& lhs, const float rhs) { return Angle(lhs.ToTurns() * rhs); }

private:
    static constexpr uint64_t kFullTurn = 1ull << 32;
    static constexpr double kFullTurnDbl = static_cast<double>(kFullTurn);
    static constexpr double kFullTurnScale = 1.0 / kFullTurnDbl;

    explicit Angle(const uint32_t turns)
        : m_Turns(turns)
    {
    }

    static uint32_t MakeTurns(const float turns)
    {
        const double dblTurns = static_cast<double>(turns);
        const double fracTurn = dblTurns - std::floor(dblTurns);
        return static_cast<uint32_t>(fracTurn * kFullTurnDbl);
    }

    uint32_t m_Turns{ 0 };
};
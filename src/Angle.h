#pragma once

#include <cmath>
// IWYU instructs clangd to keep <compare> for operator<=>, even though it thinks it is unused.
#include <compare> // IWYU pragma: keep
#include <cstdint>
#include <numbers>

class Angle
{
    static constexpr float kFullTurnRadians = 2.0f * std::numbers::pi_v<float>;
    static constexpr float kFullTurnDegrees = 360.0f;

public:
    Angle() = default;

    constexpr explicit Angle(const float turns)
        : m_Turns(MakeTurns(turns))
    {
    }

    constexpr static Angle FromRadians(const float radians)
    {
        return Angle(radians / kFullTurnRadians);
    }

    constexpr static Angle FromDegrees(const float degrees)
    {
        return Angle(degrees / kFullTurnDegrees);
    }

    constexpr float ToTurns() const { return static_cast<float>(m_Turns * kFullTurnScale); }

    constexpr float ToRadians() const { return ToTurns() * kFullTurnRadians; }

    constexpr float ToDegrees() const { return ToTurns() * kFullTurnDegrees; }

    constexpr float Cos() const { return std::cos(ToRadians()); }
    constexpr float Sin() const { return std::sin(ToRadians()); }

    constexpr Angle& operator+=(const Angle& other)
    {
        m_Turns += other.m_Turns;
        return *this;
    }

    constexpr Angle& operator-=(const Angle& other)
    {
        m_Turns -= other.m_Turns;
        return *this;
    }

    constexpr Angle operator-() const { return Angle(-ToTurns()); }

    friend constexpr auto operator<=>(const Angle& lhs, const Angle& rhs)
    {
        return lhs.m_Turns <=> rhs.m_Turns;
    }

    friend constexpr bool operator==(const Angle& lhs, const Angle& rhs)
    {
        return lhs.m_Turns == rhs.m_Turns;
    }

    friend constexpr Angle operator+(const Angle& lhs, const Angle& rhs)
    {
        return Angle(lhs.m_Turns + rhs.m_Turns);
    }

    friend constexpr Angle operator-(const Angle& lhs, const Angle& rhs)
    {
        return Angle(lhs.m_Turns - rhs.m_Turns);
    }

    friend constexpr Angle operator/(const Angle& lhs, const float rhs) { return Angle(lhs.ToTurns() / rhs); }

    friend constexpr Angle operator*(const Angle& lhs, const float rhs) { return Angle(lhs.ToTurns() * rhs); }

private:
    static constexpr uint64_t kFullTurn = 1ull << 32;
    static constexpr double kFullTurnDbl = static_cast<double>(kFullTurn);
    static constexpr double kFullTurnScale = 1.0 / kFullTurnDbl;

    constexpr explicit Angle(const uint32_t turns)
        : m_Turns(turns)
    {
    }

    // Requires a finite input.
    // Would have used std::fmodf(turns, 1.0f) if it were constexpr.
    // Or turns - std::floor(turns), but std::floor is not constexpr for float.
    static constexpr uint32_t MakeTurns(const float turns)
    {
        constexpr float kWholeTurnsOnlyThreshold = 0x1p23f; // 2^23

        // At |turns| >= 2^23, a float has no fractional part.
        // This also keeps the int32_t conversion below within range.
        if(turns >= kWholeTurnsOnlyThreshold || turns <= -kWholeTurnsOnlyThreshold)
        {
            return 0;
        }

        // Integer conversion truncates toward zero, leaving a signed fraction.
        const double value = static_cast<double>(turns);
        double fraction = value - static_cast<int32_t>(turns);

        // Wrap negative fractions into the positive turn interval.
        if(fraction < 0.0)
        {
            fraction += 1.0;
        }

        // Floating-point rounding can produce exactly 1.0, yielding 2^32.
        // uint64_t can represent that value. Narrowing to uint32_t then
        // wraps it to zero, whereas a direct float-to-uint32_t conversion
        // would be out of range.
        return static_cast<uint32_t>(static_cast<uint64_t>(fraction * kFullTurnDbl));
    }

    uint32_t m_Turns{ 0 };
};
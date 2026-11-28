#include "Random.h"

#include "Diag.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

/// Only the low 63 bits of stream select the sequence.
Random::Random(const uint64_t seed, const uint64_t stream)
    : m_Increment((stream << 1u) | 1u)
{
    // Advance from zero to the stream's increment, discarding the output.
    (void)NextU32();

    // Incorporate the seed into the stream-dependent state.
    m_State += seed;

    // Advance once more before producing the first caller-visible value.
    (void)NextU32();
}

uint32_t
Random::NextU32()
{
    const uint64_t previous = m_State;
    m_State = (previous * UINT64_C(6364136223846793005)) + m_Increment;

    const auto bits = static_cast<uint32_t>(((previous >> 18u) ^ previous) >> 27u);
    const auto rotation = static_cast<int>(previous >> 59u);

    return std::rotr(bits, rotation);
}

/// Returns an unbiased integer in [0, bound). bound must be greater than zero.
uint32_t
Random::NextBelow(const uint32_t bound)
{
    if(!GG_VERIFYB(bound != 0, "Random bound must be greater than zero"))
    {
        return 0;
    }

    // Reject the incomplete interval before applying modulo.
    const uint32_t threshold = static_cast<uint32_t>(UINT64_C(0x100000000) % bound);

    for(;;)
    {
        const uint32_t value = NextU32();
        if(value >= threshold)
        {
            return value % bound;
        }
    }
}

uint32_t
Random::NextInRange(const uint32_t min, const uint32_t max)
{
    GG_ASSERT(max >= min);

    if(min == 0 && std::numeric_limits<uint32_t>::max() == max)
    {
        return NextU32();
    }

    const uint32_t range = max - min + 1;
    return min + NextBelow(range);
}

/// Returns a float in [0, 1), in steps of 2^-24.
float
Random::NextFloat()
{
    // NOLINTBEGIN
    static_assert(std::numeric_limits<float>::is_iec559
        && std::numeric_limits<float>::radix == 2
        && std::numeric_limits<float>::digits == 24);

    return static_cast<float>(NextU32() >> 8u) * 0x1p-24f;
    // NOLINTEND
}

float
Random::NextInRange(const float min, const float max)
{
    GG_ASSERT(max > min);
    const float value = min + (NextFloat() * (max - min));

    // The contract specifies that the returned value should be in [min, max).
    // But floating-point rounding can cause the value to equal max.

    // std::nextafter returns the next representable number below max towards min.
    return std::min(value, std::nextafter(max, min));
}

bool
Random::NextBool(const float probability)
{
    GG_ASSERT(probability >= 0.0f && probability <= 1.0f);
    return NextFloat() < probability;
}
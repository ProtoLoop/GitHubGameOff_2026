#pragma once

// PCG32 XSH-RR, adapted from Melissa O'Neill's PCG minimal C implementation.
// Source: https://github.com/imneme/pcg-c-basic/blob/master/pcg_basic.c
// Algorithm: https://www.pcg-random.org/using-pcg-c-basic.html
// Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
// SPDX-License-Identifier: Apache-2.0
// License: https://www.apache.org/licenses/LICENSE-2.0
// Distributed on an "AS IS" basis, without warranties or conditions of any kind.
// See the license for permissions and limitations.
// Changes: C++ class API, std::rotr, checked bounds, and binary32 float conversion.

#include <cstdint>

/// Deterministic generator. Matching seeds, streams, and calls produce matching output.
/// Copying duplicates the current sequence position. Not for cryptographic use.
///
/// Example: create once and reuse for successive draws.
/// @code
/// Random random(42, 54); // Seed and stream.
/// const uint32_t direction = random.NextBelow(8); // 0 through 7.
/// const float fraction = random.NextFloat();     // [0, 1).
/// const bool spawn = random.NextBelow(100) < 20;  // 20% chance.
/// @endcode
class Random
{
public:
    /// Only the low 63 bits of stream select the sequence.
    Random(const uint64_t seed, const uint64_t stream);

    /// Returns the next 32-bit unsigned integer in the sequence.
    [[nodiscard]] uint32_t NextU32();

    /// Returns an unbiased integer in [0, bound). bound must be greater than zero.
    [[nodiscard]] uint32_t NextBelow(const uint32_t bound);

    /// Returns an integer in [min, max], inclusive.
    [[nodiscard]] uint32_t NextInRange(const uint32_t min, const uint32_t max);

    /// Returns a float in [0, 1).
    [[nodiscard]] float NextFloat();

    /// Returns a float in [min, max).
    [[nodiscard]] float NextInRange(const float min, const float max);

    /// Returns true with the given probability in [0, 1].
    [[nodiscard]] bool NextBool(const float probability);

private:
    uint64_t m_State{ 0 };
    uint64_t m_Increment;
};

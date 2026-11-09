#pragma once

#define scast static_cast

/// Shortcut for reinterpret_cast
template<typename T, typename U>
T
recast(U u)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<T>(u);
}

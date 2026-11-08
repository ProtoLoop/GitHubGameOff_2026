#pragma once

/// Shortcut for reinterpret_cast
template<typename T, typename U>
T
recast(U u)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<T>(u);
}

/// Shortcut for static_cast
template<typename T, typename U>
T
scast(U u)
{
    return static_cast<T>(u);
}

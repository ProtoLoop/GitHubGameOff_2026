#pragma once

#include <format>
#include <SDL3/SDL_log.h>

class Diag final
{
public:

    Diag() = delete;
    ~Diag() = default;
    Diag(const Diag&) = delete;
    Diag& operator=(const Diag&) = delete;
    Diag(Diag&&) = delete;
    Diag& operator=(Diag&&) = delete;

    template<class... Args>
    static void Error(std::format_string<Args...> pattern, Args&&... args)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "%s",
            std::format(pattern, std::forward<Args>(args)...).c_str());
    }

    /// Platform-specific check if a debugger is attached.
    static bool IsDebuggerAttached();
};

#define GG_ERR(...) Diag::Error(__VA_ARGS__)

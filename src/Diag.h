#pragma once

#include <format>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_log.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
    static void Info(
        const char* file, const int line, std::format_string<Args...> pattern, Args&&... args)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
            "%s:%d: %s",
            file,
            line,
            std::format(pattern, std::forward<Args>(args)...).c_str());
    }

    template<class... Args>
    static void Warn(
        const char* file, const int line, std::format_string<Args...> pattern, Args&&... args)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "%s:%d: %s",
            file,
            line,
            std::format(pattern, std::forward<Args>(args)...).c_str());
    }

    template<class... Args>
    static void Error(
        const char* file, const int line, std::format_string<Args...> pattern, Args&&... args)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "%s:%d: %s",
            file,
            line,
            std::format(pattern, std::forward<Args>(args)...).c_str());
    }

    /// Platform-specific check if a debugger is attached.
    static bool IsDebuggerAttached();

    static void ShowAlert();
};

#define GG_INFO(...) Diag::Info(__FILE__, __LINE__, __VA_ARGS__)
#define GG_WARN(...) Diag::Warn(__FILE__, __LINE__, __VA_ARGS__)
#define GG_ERROR(...) Diag::Error(__FILE__, __LINE__, __VA_ARGS__)

#ifdef __EMSCRIPTEN__
#define GG_DEBUG_BREAK void(emscripten_debugger())
#else
#define GG_DEBUG_BREAK void(Diag::IsDebuggerAttached() && (SDL_TriggerBreakpoint(), true))
#endif

#define GG_VERIFYB(expr, ...)                                                                      \
    (static_cast<bool>(expr) || (__VA_OPT__(GG_ERROR(__VA_ARGS__), ) GG_DEBUG_BREAK, false))

#define GG_VERIFY(expr, ...)                                                                       \
    (static_cast<bool>(expr) || (__VA_OPT__(GG_ERROR(__VA_ARGS__), ) false))

#ifndef NDEBUG
#define GG_ASSERT(expr, ...) void(GG_VERIFYB(expr __VA_OPT__(, ) __VA_ARGS__))
#else
#define GG_ASSERT(expr, ...) ((void)0)
#endif
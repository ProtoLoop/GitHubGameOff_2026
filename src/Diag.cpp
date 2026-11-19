#include "Diag.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_messagebox.h>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <fstream>
#include <sstream>
#include <string>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace
{
#if defined(__EMSCRIPTEN__)

// Use Emscripten's JavaScript interop to log messages to the browser console.

EM_JS(void, BrowserLogDebug, (const char* message), { console.debug(UTF8ToString(message)); });
EM_JS(void, BrowserLogInfo, (const char* message), { console.info(UTF8ToString(message)); });
EM_JS(void, BrowserLogWarn, (const char* message), { console.warn(UTF8ToString(message)); });
EM_JS(void, BrowserLogError, (const char* message), { console.error(UTF8ToString(message)); });

void
SDL_LogCb(void* /*userdata*/, int /*category*/, const SDL_LogPriority priority, const char* const message)
{
    switch(priority)
    {
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_DEBUG:
            BrowserLogDebug(message);
            break;
        case SDL_LOG_PRIORITY_INFO:
            BrowserLogInfo(message);
            break;
        case SDL_LOG_PRIORITY_WARN:
            BrowserLogWarn(message);
            break;
        case SDL_LOG_PRIORITY_ERROR:
        case SDL_LOG_PRIORITY_CRITICAL:
            BrowserLogError(message);
            break;
        default:
            break;
    }
 }
#endif // defined(__EMSCRIPTEN__)}
} // namespace

bool
Diag::IsDebuggerAttached()
{
#if defined(_WIN32)
    return IsDebuggerPresent() != 0;

#elif defined(__linux__)
    std::ifstream status{ "/proc/self/status" };
    if(!status)
    {
        return false;
    }

    std::string line;
    while(std::getline(status, line))
    {
        if(line.compare(0, 10, "TracerPid:") == 0)
        {
            int tracer_pid = 0;
            std::istringstream{ line.substr(10) } >> tracer_pid;
            return tracer_pid != 0;
        }
    }
    return false;

#elif defined(__APPLE__)
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    kinfo_proc info{};
    size_t size = sizeof(info);

    if(sysctl(&mib[0], 4, &info, &size, nullptr, 0) == -1)
    {
        return false;
    }

    const bool is_traced = (info.kp_proc.p_flag & P_TRACED) != 0;
    return is_traced;

#else
    return false;
#endif
}

void
Diag::ShowAlert()
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "There was an error", nullptr);
}

void
Diag::InstallLogOutputInterceptor()
{
#if defined(__EMSCRIPTEN__)
    static bool installed = false;
    if(!installed)
    {
        SDL_SetLogOutputFunction(SDL_LogCb, nullptr);
        installed = true;
    }
#endif // defined(__EMSCRIPTEN__)
}
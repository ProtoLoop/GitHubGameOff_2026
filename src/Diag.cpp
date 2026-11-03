#include "Diag.h"

#include <optional>

#if defined(_WIN32)
    #include <Windows.h>
#elif defined(__linux__)
    #include <fstream>
    #include <sstream>
    #include <string>
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#endif

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

    return (info.kp_proc.p_flag & P_TRACED) != 0;

#else
    return false;
#endif
}
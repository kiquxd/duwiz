#include "system_opener.h"

#include <spawn.h>
#include <sys/wait.h>

#include <cerrno>
#include <cstring>
#include <thread>

extern char** environ;

OpenResult OpenWithSystemViewer(const std::filesystem::path& path) {
#if defined(__APPLE__)
    const char* command = "open";
#elif defined(__linux__)
    const char* command = "xdg-open";
#else
    return {false, "system viewer is unsupported on this platform"};
#endif

    const std::string native_path = path.string();
    char* arguments[] = {
        const_cast<char*>(command),
        const_cast<char*>(native_path.c_str()),
        nullptr,
    };
    pid_t child = 0;
    const int error = ::posix_spawnp(&child, command, nullptr, nullptr,
                                     arguments, environ);
    if (error != 0) {
        return {false, std::string("cannot start ") + command + ": " +
                           std::strerror(error)};
    }

    std::thread([child] {
        int status = 0;
        while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
    }).detach();
    return {true, {}};
}

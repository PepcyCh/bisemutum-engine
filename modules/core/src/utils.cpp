#include "core/utils.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

BISMUTH_NAMESPACE_BEGIN

std::filesystem::path CurrentExecutablePath() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return path;
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::string(result, count > 0 ? count : 0);
#endif
}

BISMUTH_NAMESPACE_END

#include <bisemutum/platform/exe_dir.hpp>

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace bi {

auto get_executable_directory() -> std::filesystem::path {
#ifdef WIN32
    wchar_t path_wchars[MAX_PATH];
    GetModuleFileNameW(nullptr, path_wchars, MAX_PATH);
    std::filesystem::path path{path_wchars};
    return path.parent_path();
#else
    char path_chars[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path_chars, PATH_MAX);
    std::filesystem::path path{std::string{path_chars, count > 0 ? count : 0}};
    return path.parent_path();
#endif
}

}

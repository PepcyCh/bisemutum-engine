#pragma once

#include <filesystem>

namespace bi {

auto get_executable_directory() -> std::filesystem::path;

}

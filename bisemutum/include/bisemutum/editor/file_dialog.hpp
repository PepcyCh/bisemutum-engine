#pragma once

#include <functional>
#include <string>

#include "../prelude/idiom.hpp"

namespace bi::editor {

struct FileDialog final : PImpl<FileDialog> {
    struct Impl;

    FileDialog();

    FileDialog(FileDialog&& rhs);
    auto operator=(FileDialog&& rhs) -> FileDialog&;

    auto update() -> void;

    using ChooseFileCallback = std::function<auto(std::string) -> void>;
    auto choose_file(char const* key, char const* title, char const* filter, ChooseFileCallback callback) -> void;
};

}

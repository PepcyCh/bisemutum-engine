#pragma once

#include <functional>
#include <string>

#include "../prelude/bitflags.hpp"
#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"

namespace bi {

struct EditorDisplayer;

}

namespace bi::editor {

struct FileDialog;

enum class MenuItemFlag : uint8_t {
    checkable = 0x1,
};

struct MenuActionContext final {
    bool state;
    Ref<FileDialog> file_dialog;
};
using MenuAction = std::function<auto(MenuActionContext const&) -> void>;

struct MenuManager final : PImpl<MenuManager> {
    struct Impl;

    MenuManager();
    ~MenuManager();

    auto register_action(std::string_view path, BitFlags<MenuItemFlag> flags, MenuAction action) -> bool;

private:
    friend EditorDisplayer;
    auto display(MenuActionContext& ctx) -> void;
};

}

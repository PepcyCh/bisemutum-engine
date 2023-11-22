#pragma once

#include "../prelude/ref.hpp"
#include "../editor/menu_manager.hpp"

namespace bi::editor {

auto register_menu_actions_runtime(Ref<editor::MenuManager> mgr) -> void;

}

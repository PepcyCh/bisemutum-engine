#include "register.hpp"

#include <bisemutum/runtime/menu_action.hpp>
#include <bisemutum/scene_basic/menu_actions.hpp>

namespace bi::editor {

auto register_menu_actions(Ref<MenuManager> mgr) -> void {
    register_menu_actions_runtime(mgr);
    register_menu_actions_scene_basic(mgr);
}

}

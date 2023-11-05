#include "register.hpp"

#include <bisemutum/scene_basic/menu_actions.hpp>

namespace bi {

auto register_menu_actions(Ref<editor::MenuManager> mgr) -> void {
    editor::register_menu_actions_scene_basic(mgr);
}

}

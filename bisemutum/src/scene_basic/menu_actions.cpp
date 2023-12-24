#include <bisemutum/scene_basic/menu_actions.hpp>

#include "menu_actions/add_light.hpp"

#include "menu_actions/import_model.hpp"
#include "menu_actions/import_texture.hpp"

namespace bi::editor {

auto register_menu_actions_scene_basic(Ref<editor::MenuManager> mgr) -> void {
    mgr->register_action("Scene/Add/Skybox", {}, menu_action_scene_add_skybox);

    mgr->register_action("Asset/Import/Model (Assimp)", {}, menu_action_import_model_assimp);
    mgr->register_action("Asset/Import/Texture HDRI", {}, menu_action_import_texture_hdri);
}

}

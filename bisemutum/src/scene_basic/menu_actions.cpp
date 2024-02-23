#include <bisemutum/scene_basic/menu_actions.hpp>

#include "menu_actions/add_light.hpp"
#include "menu_actions/add_volume.hpp"

#include "menu_actions/import_model.hpp"
#include "menu_actions/import_texture.hpp"

namespace bi::editor {

auto register_menu_actions_scene_basic(Ref<editor::MenuManager> mgr) -> void {
    mgr->register_action("Scene/Add/Light/Dir Light", {}, menu_action_scene_add_dir_light);
    mgr->register_action("Scene/Add/Light/Point Light", {}, menu_action_scene_add_point_light);
    mgr->register_action("Scene/Add/Light/Spot Light", {}, menu_action_scene_add_spot_light);
    mgr->register_action("Scene/Add/Light/Rect Light", {}, menu_action_scene_add_rect_light);
    mgr->register_action("Scene/Add/Light/Skybox", {}, menu_action_scene_add_skybox);

    mgr->register_action("Scene/Add/Volume/Post Process", {}, menu_action_scene_add_post_process_volume);

    mgr->register_action("Asset/Import/Model (Assimp)", {}, menu_action_import_model_assimp);
    mgr->register_action("Asset/Import/Texture/HDRI", {}, menu_action_import_texture_hdri);
    mgr->register_action("Asset/Import/Texture/2D Texture", {}, menu_action_import_texture_2d);
}

}

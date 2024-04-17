#pragma once

#include <bisemutum/scene_basic/menu_actions.hpp>

namespace bi::editor {

auto menu_action_import_model_gltf(MenuActionContext const& ctx) -> void;

auto menu_action_import_model_assimp(MenuActionContext const& ctx) -> void;

}

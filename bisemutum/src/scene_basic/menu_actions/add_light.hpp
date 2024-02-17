#pragma once

#include <bisemutum/scene_basic/menu_actions.hpp>

namespace bi::editor {

auto menu_action_scene_add_dir_light(MenuActionContext const& ctx) -> void;

auto menu_action_scene_add_point_light(MenuActionContext const& ctx) -> void;

auto menu_action_scene_add_spot_light(MenuActionContext const& ctx) -> void;

auto menu_action_scene_add_rect_light(MenuActionContext const& ctx) -> void;

auto menu_action_scene_add_skybox(MenuActionContext const& ctx) -> void;

}

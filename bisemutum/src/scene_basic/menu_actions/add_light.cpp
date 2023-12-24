#include "add_light.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi::editor {

auto menu_action_scene_add_skybox(MenuActionContext const& ctx) -> void {
    auto scene = g_engine->world()->current_scene();
    auto object = scene->create_scene_object();
    object->set_name("Skybox");
    object->attach_component(SkyboxComponent{});
}

}

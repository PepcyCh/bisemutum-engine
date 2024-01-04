#include "add_volume.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/scene_basic/post_process_volume.hpp>

namespace bi::editor {

auto menu_action_scene_add_post_process_volume(MenuActionContext const& ctx) -> void {
    auto scene = g_engine->world()->current_scene();
    auto object = scene->create_scene_object();
    object->set_name("Post Process Volume");
    object->attach_component(PostProcessVolumeComponent{});
}

}

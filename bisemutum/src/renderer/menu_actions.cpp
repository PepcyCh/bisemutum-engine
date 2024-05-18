#include <bisemutum/renderer/menu_actions.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/renderer/post_process_volume.hpp>

namespace bi::editor {

namespace {

auto menu_action_scene_add_post_process_volume(MenuActionContext const& ctx) -> void {
    auto scene = g_engine->world()->current_scene();
    auto object = scene->create_scene_object();
    object->set_name("Post Process Volume");
    object->attach_component(PostProcessVolumeComponent{});
}

} // namespace

auto register_menu_actions_basic_renderer(Ref<editor::MenuManager> mgr) -> void {
    mgr->register_action("Scene/Add/Volume/Post Process", {}, menu_action_scene_add_post_process_volume);
}

}

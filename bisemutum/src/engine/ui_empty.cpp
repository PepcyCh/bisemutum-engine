#include "ui_empty.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/ecs.hpp>
#include <bisemutum/scene_basic/camera_system.hpp>

namespace bi {

// TODO - merger displayer & ui
auto EmptyEngineUI::execute() -> void {
    auto camera_system = g_engine->ecs_manager()->get_system<CameraSystem>();
    displayer_.set_camera(camera_system->main_camera_handle());
}

}

#include "ui_empty.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/ecs.hpp>
#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/scene_basic/camera_system.hpp>

namespace bi {

EmptyEngineUI::EmptyEngineUI() {
    g_engine->ecs_manager()->ecs_registry().on_construct<CameraComponent>().connect<&EmptyEngineUI::on_construct>(this);
    g_engine->ecs_manager()->ecs_registry().on_destroy<CameraComponent>().connect<&EmptyEngineUI::on_destroy>(this);
}

auto EmptyEngineUI::execute() -> void {
    if (displayed_camera_ == gfx::CameraHandle::invalid && displayed_camera_entity_ != static_cast<entt::entity>(-1)) {
        auto camera_system = g_engine->ecs_manager()->get_system<CameraSystem>();
        displayed_camera_ = camera_system->camera_handle_of(displayed_camera_entity_);
        displayer_.set_camera(displayed_camera_);
    }
}

auto EmptyEngineUI::on_construct(entt::registry& ecs_registry, entt::entity entity) -> void {
    camera_entities_.insert(entity);
    if (displayed_camera_ == gfx::CameraHandle::invalid) {
        displayed_camera_entity_ = entity;
    }
}

auto EmptyEngineUI::on_destroy(entt::registry& ecs_registry, entt::entity entity) -> void {
    camera_entities_.erase(entity);
    if (displayed_camera_entity_ == entity) {
        if (camera_entities_.empty()) {
            displayed_camera_ = gfx::CameraHandle::invalid;
            displayed_camera_entity_ = static_cast<entt::entity>(-1);
        } else {
            displayed_camera_entity_ = *camera_entities_.begin();
            auto camera_system = g_engine->ecs_manager()->get_system<CameraSystem>();
            displayed_camera_ = camera_system->camera_handle_of(displayed_camera_entity_);
        }
        displayer_.set_camera(displayed_camera_);
    }
}

}

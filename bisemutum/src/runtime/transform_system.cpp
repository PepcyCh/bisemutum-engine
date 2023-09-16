#include <bisemutum/runtime/transform_system.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/runtime/ecs.hpp>

namespace bi::rt {

TransformSystem::TransformSystem() {
    g_engine->ecs_manager()->ecs_registry().on_update<Transform>().connect<&TransformSystem::on_transform_update>(this);
}

auto TransformSystem::update() -> void {}

auto TransformSystem::on_transform_update(entt::registry& ecs_registy, entt::entity entity) -> void {
    auto object = g_engine->ecs_manager()->scene_object_of(entity);
    object->world_transform_dirty_ = true;
}

}

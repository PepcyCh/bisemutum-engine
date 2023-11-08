#include <bisemutum/runtime/transform_system.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/scene_object.hpp>

namespace bi::rt {

auto TransformSystem::init_on(Ref<Scene> scene) -> void {
    scene_ = scene;
    scene_->ecs_registry().on_update<Transform>().connect<&TransformSystem::on_transform_update>(this);
}

auto TransformSystem::update() -> void {}

auto TransformSystem::on_transform_update(entt::registry& ecs_registy, entt::entity entity) -> void {
    auto object = scene_->object_of(entity);
    object->world_transform_dirty_ = true;
}

}

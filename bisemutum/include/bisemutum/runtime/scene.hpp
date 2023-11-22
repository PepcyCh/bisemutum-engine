#pragma once

#include <list>
#include <unordered_map>
#include <functional>

#include <entt/entity/registry.hpp>

#include "../prelude/ref.hpp"
#include "../utils/serde.hpp"

namespace bi::rt {

struct SceneObject;
struct Prefab;

struct Scene final {
    auto ecs_registry() -> entt::registry&;
    auto ecs_registry() const -> entt::registry const&;
    auto object_of(entt::entity esc_entity) const -> Ref<SceneObject>;

    auto for_each_object(std::function<auto(Ref<SceneObject>) -> void> op) -> void;
    auto for_each_object(std::function<auto(CRef<SceneObject>) -> void> op) const -> void;

    auto for_each_root_object(std::function<auto(Ref<SceneObject>) -> void> op) -> void;
    auto for_each_root_object(std::function<auto(CRef<SceneObject>) -> void> op) const -> void;

    auto detach_as_root_object(Ref<SceneObject> object) -> void;
    auto attach_as_child_object(Ref<SceneObject> object, Ref<SceneObject> parent) -> void;
    auto remove_object(Ref<SceneObject> object) -> void;
    auto remove_root_object(Ref<SceneObject> object) -> void;

    auto create_scene_object(Ptr<SceneObject> parent = nullptr) -> Ref<SceneObject>;
    auto destroy_scene_object(Ref<SceneObject> object) -> void;
    auto destroy_scene_object_and_its_children(Ref<SceneObject> object) -> void;
    auto do_destroy_scene_objects() -> void;

    auto load_from_value(serde::Value &&value) -> void;

private:
    friend Prefab;
    auto create_scene_object(Ptr<SceneObject> parent, bool with_transform) -> Ref<SceneObject>;

    entt::registry ecs_registry_;
    std::unordered_map<entt::entity, Ref<SceneObject>> entity_map_;

    std::list<SceneObject> objects_;
    std::list<Ref<SceneObject>> root_objects_;
    std::unordered_map<SceneObject*, std::list<SceneObject>::iterator> objects_it_map_;
    std::unordered_map<SceneObject*, std::list<Ref<SceneObject>>::iterator> root_objects_it_map_;
    std::vector<Ref<SceneObject>> destroyed_objects_;
};

}

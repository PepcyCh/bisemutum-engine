#pragma once

#include "../prelude/ref.hpp"
#include "../prelude/idiom.hpp"

namespace bi::rt {

struct Scene;
struct SceneObject;

struct World final : PImpl<World> {
    struct Impl;

    World();
    ~World();

    auto current_scene() -> Ptr<Scene>;
    auto current_scene() const -> CPtr<Scene>;

    auto create_scene() -> Ref<Scene>;
    auto destroy_scene(Ref<Scene> scene) -> void;

    auto create_scene_object(Ref<Scene> scene, Ptr<SceneObject> parent = nullptr) -> Ref<SceneObject>;
    auto destroy_scene_object(Ref<SceneObject> object) -> void;
    auto destroy_scene_object_and_its_children(Ref<SceneObject> object) -> void;
    auto do_destroy_scene_objects() -> void;

    auto load_scene(std::string_view scene_json_str) -> bool;
};

}

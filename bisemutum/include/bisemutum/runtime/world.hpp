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

    auto create_scene(bool is_dummy_scene = false) -> Ref<Scene>;
    auto destroy_scene(Ref<Scene> scene) -> void;

    auto load_scene(std::string_view scene_file_str) -> bool;
};

}

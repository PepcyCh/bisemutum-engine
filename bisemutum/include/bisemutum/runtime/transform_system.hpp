#pragma once

#include "scene.hpp"

namespace bi::rt {

struct TransformSystem final {
    auto init_on(Ref<Scene> scene) -> void;
    auto update() -> void;

    auto on_transform_update(entt::registry& ecs_registy, entt::entity entity) -> void;

private:
    Ptr<Scene> scene_;
};

}

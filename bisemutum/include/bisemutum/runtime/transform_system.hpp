#pragma once

#include <entt/entity/registry.hpp>

namespace bi::rt {

struct TransformSystem final {
    TransformSystem();

    auto update() -> void;

    auto on_transform_update(entt::registry& ecs_registy, entt::entity entity) -> void;
};

}

#pragma once

#include <entt/entity/registry.hpp>

#include "../prelude/idiom.hpp"
#include "../runtime/scene.hpp"
#include "../graphics/handles.hpp"

namespace bi {

struct CameraSystem final : PImpl<CameraSystem> {
    struct Impl;

    CameraSystem();

    auto init_on(Ref<rt::Scene> scene) -> void;
    auto update() -> void;

    auto main_camera_handle() const -> gfx::CameraHandle;
    auto camera_handle_of(entt::entity entity) const -> gfx::CameraHandle;
};

}

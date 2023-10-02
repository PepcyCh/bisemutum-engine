#pragma once

#include <entt/entity/registry.hpp>

#include "../prelude/idiom.hpp"
#include "../graphics/handles.hpp"

namespace bi {

struct CameraSystem final : PImpl<CameraSystem> {
    struct Impl;

    CameraSystem();
    ~CameraSystem();

    CameraSystem(CameraSystem&& rhs) noexcept;
    auto operator=(CameraSystem&& rhs) noexcept -> CameraSystem&;

    auto update() -> void;

    auto main_camera_handle() const -> gfx::CameraHandle;
    auto camera_handle_of(entt::entity entity) const -> gfx::CameraHandle;
};

}

#pragma once

#include "../prelude/idiom.hpp"

namespace bi {

struct CameraSystem final : PImpl<CameraSystem> {
    struct Impl;

    CameraSystem();
    ~CameraSystem();

    CameraSystem(CameraSystem&& rhs) noexcept;
    auto operator=(CameraSystem&& rhs) noexcept -> CameraSystem&;

    auto update() -> void;
};

}

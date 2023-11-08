#pragma once

#include "../prelude/idiom.hpp"
#include "../runtime/scene.hpp"

namespace bi {

struct StaticMeshRenderSystem final : PImpl<StaticMeshRenderSystem> {
    struct Impl;

    StaticMeshRenderSystem();
    ~StaticMeshRenderSystem();

    StaticMeshRenderSystem(StaticMeshRenderSystem&& rhs) noexcept;
    auto operator=(StaticMeshRenderSystem&& rhs) noexcept -> StaticMeshRenderSystem&;

    auto init_on(Ref<rt::Scene> scene) -> void;
    auto update() -> void;
};

}

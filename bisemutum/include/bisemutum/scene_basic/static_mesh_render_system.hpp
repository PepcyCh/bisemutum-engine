#pragma once

#include "../prelude/idiom.hpp"

namespace bi {

struct StaticMeshRenderSystem final : PImpl<StaticMeshRenderSystem> {
    struct Impl;

    StaticMeshRenderSystem();
    ~StaticMeshRenderSystem();

    StaticMeshRenderSystem(StaticMeshRenderSystem&& rhs) noexcept;
    auto operator=(StaticMeshRenderSystem&& rhs) noexcept -> StaticMeshRenderSystem&;

    auto update() -> void;
};

}

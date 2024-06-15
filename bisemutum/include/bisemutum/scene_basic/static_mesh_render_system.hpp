#pragma once

#include "../prelude/idiom.hpp"
#include "../runtime/scene.hpp"

namespace bi {

struct StaticMeshRenderSystem final : PImpl<StaticMeshRenderSystem> {
    struct Impl;

    StaticMeshRenderSystem();

    auto init_on(Ref<rt::Scene> scene) -> void;
    auto update() -> void;
};

}

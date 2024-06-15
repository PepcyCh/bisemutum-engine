#pragma once

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"

namespace bi::rt {

struct Scene;

struct PrefabManager final : PImpl<PrefabManager> {
    struct Impl;

    PrefabManager();

    auto update() -> void;

    auto scene() const -> Ref<rt::Scene>;
};

}

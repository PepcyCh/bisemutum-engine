#pragma once

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"

namespace bi::rt {

struct Scene;

struct PrefabManager final : PImpl<PrefabManager> {
    struct Impl;

    PrefabManager();
    ~PrefabManager();

    PrefabManager(PrefabManager&& rhs) noexcept;
    auto operator=(PrefabManager&& rhs) noexcept -> PrefabManager&;

    auto update() -> void;

    auto scene() const -> Ref<rt::Scene>;
};

}

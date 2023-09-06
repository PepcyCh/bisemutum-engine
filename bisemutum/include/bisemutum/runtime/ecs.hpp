#pragma once

#include <typeindex>

#include <entt/entity/registry.hpp>

#include "../prelude/idiom.hpp"
#include "../prelude/poly.hpp"

namespace bi::rt {

BI_TRAIT_BEGIN(ISystem, move)
    BI_TRAIT_METHOD(update, (&self) requires (self.update()) -> void)
BI_TRAIT_END(ISystem)

struct EcsManager final : PImpl<EcsManager> {
    struct Impl;

    EcsManager();
    ~EcsManager();

    auto get_ecs_registry() -> entt::registry&;
    auto get_ecs_registry() const -> entt::registry const&;

    template <typename System>
    auto register_system() -> void {
        register_system(typeid(System), System{});
    }

    auto tick() -> void;

private:
    auto register_system(std::type_index type, Dyn<ISystem>::Box system) -> void;
};

}

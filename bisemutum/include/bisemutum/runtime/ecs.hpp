#pragma once

#include <typeindex>

#include <entt/entity/registry.hpp>

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"
#include "../prelude/poly.hpp"

namespace bi::rt {

BI_TRAIT_BEGIN(ISystem, move)
    BI_TRAIT_METHOD(update, (&self) requires (self.update()) -> void)
BI_TRAIT_END(ISystem)

struct SceneObject;

struct EcsManager final : PImpl<EcsManager> {
    struct Impl;

    EcsManager();
    ~EcsManager();

    auto ecs_registry() -> entt::registry&;
    auto ecs_registry() const -> entt::registry const&;

    auto scene_object_of(entt::entity entity) const -> Ref<SceneObject>;

    template <typename System>
    auto register_system() -> void {
        register_system(typeid(System), System{});
    }

    auto tick() -> void;

private:
    auto register_system(std::type_index type, Dyn<ISystem>::Box system) -> void;

    friend SceneObject;
    auto add_scene_object(Ref<SceneObject> object, entt::entity entity) -> void;
    auto remove_scene_object(entt::entity entity) -> void;
};

}

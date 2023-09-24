#pragma once

#include <typeindex>

#include <entt/entity/registry.hpp>

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"
#include "../prelude/poly.hpp"

namespace bi::rt {

BI_TRAIT_BEGIN(ISystem, move, type_info)
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
        // Dyn<ISystem>::Box sys;
        // sys.emplace<System>();
        // register_system(typeid(System), std::move(sys));
        register_system(typeid(System), make_poly<ISystem, System>());
    }
    template <typename System>
    auto get_system() const -> CPtr<System> {
        return aa::any_cast<System>(get_system(typeid(System)));
    }

    auto tick() -> void;

private:
    auto register_system(std::type_index type, Dyn<ISystem>::Box system) -> void;
    auto get_system(std::type_index type) const -> Dyn<ISystem>::CPtr;

    friend SceneObject;
    auto add_scene_object(Ref<SceneObject> object, entt::entity entity) -> void;
    auto remove_scene_object(entt::entity entity) -> void;
};

}

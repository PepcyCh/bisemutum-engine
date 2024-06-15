#pragma once

#include <typeindex>
#include <functional>

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"
#include "../prelude/poly.hpp"

namespace bi::rt {

struct Scene;

BI_TRAIT_BEGIN(IGlobalSystem, move, type_info)
    template <typename T>
    static auto helper_update(T& self) -> void {}
    template <typename T> requires requires (T v) { v.update(); }
    static auto helper_update(T& self) -> void {
        return self.update();
    }

    template <typename T>
    static auto helper_post_update(T& self) -> void {}
    template <typename T> requires requires (T v) { v.post_update(); }
    static auto helper_post_update(T& self) -> void {
        return self.post_update();
    }

    BI_TRAIT_METHOD(update, (&self) requires (helper_update(self)) -> void)
    BI_TRAIT_METHOD(post_update, (&self) requires (helper_post_update(self)) -> void)
BI_TRAIT_END(IGlobalSystem)

BI_TRAIT_BEGIN(ISystem, move, type_info)
    template <typename T>
    static auto helper_update(T& self) -> void {}
    template <typename T> requires requires (T v) { v.update(); }
    static auto helper_update(T& self) -> void {
        return self.update();
    }

    template <typename T>
    static auto helper_post_update(T& self) -> void {}
    template <typename T> requires requires (T v) { v.post_update(); }
    static auto helper_post_update(T& self) -> void {
        return self.post_update();
    }

    BI_TRAIT_METHOD(init_on, (&self, Ref<Scene> scene) requires (self.init_on(scene)) -> void)
    BI_TRAIT_METHOD(update, (&self) requires (helper_update(self)) -> void)
    BI_TRAIT_METHOD(post_update, (&self) requires (helper_post_update(self)) -> void)
BI_TRAIT_END(ISystem)

using GlobalSystemCreator = std::function<auto() -> Dyn<IGlobalSystem>::Box>;
using SystemCreator = std::function<auto() -> Dyn<ISystem>::Box>;

struct SystemManager final : PImpl<SystemManager> {
    struct Impl;

    SystemManager();

    template <typename GlobalSystem>
    auto register_global_system() -> void {
        register_global_system(typeid(GlobalSystem), []() -> Dyn<IGlobalSystem>::Box {
            return make_poly<IGlobalSystem, GlobalSystem>();
        });
    }
    template <typename GlobalSystem>
    auto get_global_system() -> CPtr<GlobalSystem> {
        return aa::any_cast<GlobalSystem>(get_global_system(typeid(GlobalSystem)));
    }

    template <typename System>
    auto register_system() -> void {
        register_system(typeid(System), []() -> Dyn<ISystem>::Box {
            return make_poly<ISystem, System>();
        });
    }

    template <typename System>
    auto get_system_for_current_scene() -> Ptr<System> {
        return aa::any_cast<System>(get_system_for_current_scene(typeid(System)));
    }
    template <typename System>
    auto get_system_for(Ref<Scene> scene) -> Ptr<System> {
        return aa::any_cast<System>(get_system_for(scene, typeid(System)));
    }

    auto init_on(Ref<Scene> scene) -> void;
    auto tick_update() -> void;
    auto tick_post_update() -> void;

private:
    auto register_global_system(std::type_index type, GlobalSystemCreator creator) -> void;
    auto get_global_system(std::type_index type) -> Dyn<IGlobalSystem>::Ptr;

    auto register_system(std::type_index type, SystemCreator creator) -> void;
    auto get_system_for_current_scene(std::type_index type) -> Dyn<ISystem>::Ptr;
    auto get_system_for(Ref<Scene> scene, std::type_index type) -> Dyn<ISystem>::Ptr;
};

}

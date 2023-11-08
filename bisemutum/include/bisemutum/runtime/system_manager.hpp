#pragma once

#include <typeindex>
#include <functional>

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"
#include "../prelude/poly.hpp"

namespace bi::rt {

struct Scene;

BI_TRAIT_BEGIN(ISystem, move, type_info)
    BI_TRAIT_METHOD(init_on, (&self, Ref<Scene> scene) requires (self.init_on(scene)) -> void)
    BI_TRAIT_METHOD(update, (&self) requires (self.update()) -> void)
BI_TRAIT_END(ISystem)

using SystemCreator = std::function<auto() -> Dyn<ISystem>::Box>;

struct SystemManager final : PImpl<SystemManager> {
    struct Impl;

    SystemManager();
    ~SystemManager();

    template <typename System>
    auto register_system() -> void {
        register_system(typeid(System), []() -> Dyn<ISystem>::Box {
            return make_poly<ISystem, System>();
        });
    }
    template <typename System>
    auto get_system() const -> CPtr<System> {
        return aa::any_cast<System>(get_system(typeid(System)));
    }

    auto init_on(Ref<Scene> scene) -> void;
    auto tick() -> void;

private:
    auto register_system(std::type_index type, SystemCreator creator) -> void;
    auto get_system(std::type_index type) const -> Dyn<ISystem>::CPtr;
};

}

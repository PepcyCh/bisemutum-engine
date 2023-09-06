#pragma once

#include "../prelude/box.hpp"
#include "../prelude/version.hpp"
#include "../prelude/poly.hpp"

namespace bi::rt {

struct ModuleInfo final {
    char const* name;
    Version version;
};

BI_TRAIT_BEGIN(IModule, move)
    BI_TRAIT_METHOD(initialize, (&self) requires (self.initialize()) -> bool)
    BI_TRAIT_METHOD(finalize, (&self) requires (self.finalize()) -> bool)
    BI_TRAIT_METHOD(info, (const& self) requires (self.info()) -> ModuleInfo const&)
BI_TRAIT_END(IModule)

struct ModuleManager final : public PImpl<ModuleManager> {
    struct Impl;

    ModuleManager();
    ~ModuleManager();

    auto initialize() -> bool;
    auto finalize() -> bool;

    auto enable_module(Dyn<IModule>::Box module) -> bool;
    auto disable_module(std::string_view name) -> void;
    auto get_module(std::string_view name) const -> Dyn<IModule>::CRef;
    auto try_get_module(std::string_view name) const -> Dyn<IModule>::CPtr;

    template <typename T>
    auto get_module_typed(std::string_view name) const -> CRef<T> {
        return aa::any_cast<T>(get_module(name));
    }
    template <typename T>
    auto try_get_module_typed(std::string_view name) const -> CPtr<T> {
        if (auto module_dyn = try_get_module(name); module_dyn) {
            if (auto module = aa::any_cast<T>(module_dyn); module) {
                return {*module};
            }
        }
        return {};
    }
};

}

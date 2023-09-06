#include <bisemutum/runtime/module.hpp>

#include <filesystem>

#include <bisemutum/containers/hash.hpp>

namespace bi::rt {

struct ModuleManager::Impl final {
    auto initialize() -> bool { return true; }
    auto finalize() -> bool { return true; }

    auto enable_module(Dyn<IModule>::Box&& module) -> bool {
        auto& info = module.info();
        if (auto it = modules.find(info.name); it != modules.end()) {
            return false;
        }
        module.initialize();
        modules.insert({std::string{info.name}, std::move(module)});
        return true;
    }
    auto disable_module(std::string_view name) -> void {
        if (auto it = modules.find(name); it != modules.end()) {
            it->second.finalize();
            modules.erase(it);
        }
    }
    auto get_module(std::string_view name) const -> Dyn<IModule>::CRef {
        return *&modules.find(name)->second;
    }
    auto try_get_module(std::string_view name) const -> Dyn<IModule>::CPtr {
        if (auto it = modules.find(name); it != modules.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }

    StringHashMap<Dyn<IModule>::Box> modules;
};

ModuleManager::ModuleManager() = default;

ModuleManager::~ModuleManager() = default;

auto ModuleManager::initialize() -> bool {
    return impl()->initialize();
}
auto ModuleManager::finalize() -> bool {
    return impl()->finalize();
}

auto ModuleManager::enable_module(Dyn<IModule>::Box module) -> bool {
    return impl()->enable_module(std::move(module));
}
auto ModuleManager::disable_module(std::string_view name) -> void {
    impl()->disable_module(name);
}
auto ModuleManager::get_module(std::string_view name) const -> Dyn<IModule>::CRef {
    return impl()->get_module(name);
}
auto ModuleManager::try_get_module(std::string_view name) const -> Dyn<IModule>::CPtr {
    return impl()->try_get_module(name);
}

}

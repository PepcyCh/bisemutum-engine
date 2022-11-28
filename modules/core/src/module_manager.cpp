#include "core/module_manager.hpp"

BISMUTH_NAMESPACE_BEGIN

ModuleManager &ModuleManager::Get() {
    static ModuleManager module_mgr {};
    return module_mgr;
}

Ref<Module> ModuleManager::Get(const char *name) {
    return ModuleManager::Get().GetModule(name);
}

BISMUTH_NAMESPACE_END

#pragma once

#include "module.hpp"
#include "container.hpp"
#include "ptr.hpp"

BISMUTH_NAMESPACE_BEGIN

class ModuleManager final {
public:
    static ModuleManager &Get();

    template <IsModule T>
    static void Load() {
        Get().LoadModule<T>();
    }

    static Ref<Module> Get(const char *name);

    template <IsModule T>
    static Ref<T> Get() {
        return Get(T::kName).template CastTo<T>();
    }

    template <IsModule T>
    void LoadModule() {
        modules_.insert({ T::kName, Ptr<T>::Make() });
    }

    Ref<Module> GetModule(const char *name) const {
        return modules_.at(name).AsRef();
    }

private:
    ModuleManager();

    HashMap<const char *, Ptr<Module>> modules_;
};

BISMUTH_NAMESPACE_END

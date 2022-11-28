#include <runtime/component.hpp>

#include <core/module_manager.hpp>
#include <nlohmann/json.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

ComponentManager::~ComponentManager() = default;

void ComponentManager::Deserialize(SceneObject &object, const nlohmann::json &value) {
    const auto &type = value["type"].get<std::string>();
    if (auto it = deserializers_.find(type.c_str()); it != deserializers_.end()) {
        it->second(object, value);
    } else {
        BI_CRTICAL(ModuleManager::Get<RuntimeModule>()->Lgr(),
            "try to deserialize a component with unregistered type '{}'", type);
    }
}

BISMUTH_RT_NAMESPACE_END

BISMUTH_RT_NAMESPACE_END

#include <runtime/object.hpp>

#include <core/module_manager.hpp>
#include <nlohmann/json.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

SceneObject::SceneObject(Ref<Scene> scene, std::string_view name, entt::registry &registry)
    : scene_(scene), name_(name), registry_(registry) {
    entity_ = registry.create();
}

nlohmann::json SceneObject::Serialize() const {
    auto object_json = nlohmann::json {
        { "name", name_ },
        { "components", {} },
    };
    auto &components_json = object_json.at("components");
    for (const auto component : components_) {
        components_json.push_back(component->Serialize());
    }
    return object_json;
}

void SceneObject::Deserialize(const nlohmann::json &value) {
    const auto &components_json = value.at("components");
    components_.reserve(components_json.size());
    for (const auto &component_json : components_json) {
        ModuleManager::Get<RuntimeModule>()->ComponentMgr()->Deserialize(*this, component_json);
    }
}

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END

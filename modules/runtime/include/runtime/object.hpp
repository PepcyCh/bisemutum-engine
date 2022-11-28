#pragma once

#include <core/container.hpp>
#include <core/ptr.hpp>
#include <entt/entity/registry.hpp>

#include "component.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

class Scene;

class SceneObject final : public RefFromThis<SceneObject> {
public:
    const std::string &GetName() const { return name_; }
    void SetName(std::string_view name) { name_ = name; }

    template <IsComponent T, typename... Args>
    T *AddComponent(Args &&... args) {
        if (registry_.all_of<T>(entity_)) {
            return nullptr;
        }
        auto &comp = registry_.emplace<T>(entity_, std::forward<Args>(args)...);
        components_.push_back(&comp);
        components_.back()->object_ = RefThis();
        components_.back()->Init();
        return &comp;
    }

    template <IsComponent T>
    T *GetComponent() const {
        return registry_.try_get<T>(entity_);
    }

    template <IsComponent T>
    bool HasComponent() const {
        return registry_.all_of<T>(entity_);
    }
    template <IsComponent... Ts>
    bool HasComponents() const {
        return registry_.all_of<Ts...>(entity_);
    }

    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json &value);

private:
    friend Scene;
    SceneObject(Ref<Scene> scene, std::string_view name, entt::registry &registry);
    Ref<Scene> scene_;

    std::string name_;

    entt::registry &registry_;
    entt::entity entity_;
    Vec<Component *> components_;
};

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END

#pragma once

#include <string>
#include <functional>

#include <entt/entity/registry.hpp>

#include "component.hpp"
#include "../math/transform.hpp"
#include "../prelude/ref.hpp"

namespace bi::rt {

struct Scene;
struct TransformSystem;

struct SceneObject final {
    SceneObject(Ref<Scene> scene, bool with_transform = true);
    SceneObject(Ref<Scene> scene, Transform transform);
    ~SceneObject();

    enum class Id : uint32_t {};

    auto get_name() const -> std::string_view { return name_; }
    auto get_name_cstr() const -> const char* { return name_.c_str(); }
    auto set_name(std::string_view name) -> void;

    auto get_id() const -> Id;

    auto world_transform() const -> Transform const&;
    auto local_transform() const -> Transform const&;

    auto is_enabled() const -> bool { return enabled_; }
    auto set_enabled(bool enabled) -> void;
    auto is_destroyed() const -> bool { return destroyed_; }
    auto mark_as_destroyed() -> void;

    auto parent() -> Ptr<SceneObject> { return parent_; }
    auto parent() const -> CPtr<SceneObject> { return parent_; }

    auto first_child() -> Ptr<SceneObject> { return first_child_; }
    auto first_child() const -> CPtr<SceneObject> { return first_child_; }

    auto last_child() -> Ptr<SceneObject> { return last_child_; }
    auto last_child() const -> CPtr<SceneObject> { return last_child_; }

    auto next_sibling() -> Ptr<SceneObject> { return next_sibling_; }
    auto next_sibling() const -> CPtr<SceneObject> { return next_sibling_; }

    auto prev_sibling() -> Ptr<SceneObject> { return prev_sibling_; }
    auto prev_sibling() const -> CPtr<SceneObject> { return prev_sibling_; }

    auto add_child(Ref<SceneObject> object) -> void;
    auto attach_under(Ref<SceneObject> parent) -> void;
    auto remove_self_from_sibling_list(bool add_to_root) -> void;
    auto extract_all_children() -> void;

    auto clone(bool include_children = true, Ptr<Scene> dst_scene = nullptr) const -> Ref<SceneObject>;

    auto for_each_children(std::function<auto(Ref<SceneObject>) -> void> op) -> void;
    auto for_each_children(std::function<auto(CRef<SceneObject>) -> void> op) const -> void;

    auto ecs_entity() const -> entt::entity { return ecs_entity_; }

    template <TComponent Component>
    auto attach_component(Component&& component) -> void {
        auto& comp = ecs_registry_->emplace<Component>(ecs_entity_, std::move(component));
        components_[Component::component_type_name] = &comp;
    }
    template <TComponent Component>
    auto dettach_component() -> void {
        ecs_registry_->remove<Component>(ecs_entity_);
        components_.erase(Component::component_type_name);
    }
    auto attach_component_by_type_name(std::string_view component_type_name) -> void;

    template <TComponent Component>
    auto has_component() const -> bool { return ecs_registry_->all_of<Component>(ecs_entity_); }
    template <TComponent... Components>
    auto has_components() const -> bool { return ecs_registry_->all_of<Components...>(ecs_entity_); }
    template <TComponent... Components>
    auto has_any_component() const -> bool { return ecs_registry_->any_of<Components...>(ecs_entity_); }

    template <TComponent Component>
    auto get_component() const -> CRef<Component> { return ecs_registry_->get<Component>(ecs_entity_); }
    template <TComponent Component>
    auto try_get_component() const -> CPtr<Component> { return ecs_registry_->try_get<Component>(ecs_entity_); }

    template <TComponent Component>
    auto update_component(std::function<auto(Component&) -> void> func) -> void {
        ecs_registry_->patch<Component>(ecs_entity_, std::move(func));
    }
    template <TComponent Component>
    auto notify_component_dirty() -> void {
        ecs_registry_->patch<Component>(ecs_entity_, [](Component&) {});
    }

    auto for_each_component(std::function<auto(std::string_view, void*) -> void> func) -> void;
    auto for_each_component(std::function<auto(std::string_view, void const*) -> void> func) const -> void;

private:
    friend TransformSystem;

    Ref<Scene> scene_;
    std::string name_;

    entt::entity ecs_entity_;
    Ref<entt::registry> ecs_registry_;
    mutable Transform world_transform_;

    bool enabled_ = true;
    bool destroyed_ = false;
    mutable bool world_transform_dirty_ = true;

    Ptr<SceneObject> parent_ = nullptr;
    Ptr<SceneObject> first_child_ = nullptr;
    Ptr<SceneObject> last_child_ = nullptr;
    Ptr<SceneObject> next_sibling_ = nullptr;
    Ptr<SceneObject> prev_sibling_ = nullptr;

    std::unordered_map<std::string_view, void*> components_;
};

}

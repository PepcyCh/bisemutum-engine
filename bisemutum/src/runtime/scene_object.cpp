#include <bisemutum/runtime/scene_object.hpp>

#include <fmt/format.h>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/component_manager.hpp>

namespace bi::rt {

SceneObject::SceneObject(Ref<Scene> scene, bool with_transform)
    : scene_(scene), ecs_registry_(scene->ecs_registry())
{
    ecs_entity_ = ecs_registry_->create();
    if (with_transform) {
        attach_component(Transform{});
    }
    name_ = fmt::format("scene_object_{}", static_cast<uint32_t>(ecs_entity_));
}

SceneObject::~SceneObject() {
    ecs_registry_->destroy(ecs_entity_);
}

auto SceneObject::get_id() const -> uint32_t {
    return static_cast<uint32_t>(ecs_entity_);
}

auto SceneObject::world_transform() const -> Transform const& {
    std::vector<CRef<SceneObject>> parent_chains;
    parent_chains.push_back(unsafe_make_ref(this));
    size_t toppest_dirty_index = 0;
    for (auto p = parent_; p; p = p->parent_) {
        parent_chains.push_back(p.value());
        if (p->world_transform_dirty_) {
            toppest_dirty_index = parent_chains.size() - 1;
        }
    }
    if (toppest_dirty_index == 0 && !world_transform_dirty_) { return world_transform_; }
    auto transform = parent_chains[toppest_dirty_index]->parent_
        ? parent_chains[toppest_dirty_index]->parent_->world_transform_
        : Transform{};
    for (size_t i = toppest_dirty_index; i < parent_chains.size(); i--) {
        auto object = parent_chains[i];
        object->for_each_children([](CRef<SceneObject> ch) { ch->world_transform_dirty_ = true; });
        object->world_transform_dirty_ = false;
        object->world_transform_ = transform = transform * object->local_transform();
    }
    return world_transform_;
}

auto SceneObject::local_transform() const -> Transform const& {
    return *get_component<Transform>();
}

auto SceneObject::set_name(std::string_view name) -> void {
    name_ = name;
}

auto SceneObject::set_enabled(bool enabled) -> void {
    enabled_ = enabled;
}

auto SceneObject::mark_as_destroyed() -> void {
    destroyed_ = true;
    enabled_ = false;
    scene_->remove_root_object(unsafe_make_ref(this));
}

auto SceneObject::add_child(Ref<SceneObject> object) -> void {
    if (!first_child_) {
        first_child_ = object;
        last_child_ = object;
    } else {
        last_child_->next_sibling_ = object;
        object->prev_sibling_ = last_child_;
        last_child_ = object;
    }
    object->parent_ = unsafe_make_ref(this);
}

auto SceneObject::attach_under(Ref<SceneObject> parent) -> void {
    remove_self_from_sibling_list(false);
    if (!parent_) {
        scene_->remove_root_object(unsafe_make_ref(this));
    }
    parent->add_child(unsafe_make_ref(this));
}

auto SceneObject::remove_self_from_sibling_list(bool add_to_root) -> void {
    if (next_sibling_) {
        next_sibling_->prev_sibling_ = prev_sibling_;
    }
    if (prev_sibling_) {
        prev_sibling_->next_sibling_ = next_sibling_;
    }
    if (parent_ && parent_->first_child_ == this) {
        parent_->first_child_ = next_sibling_;
    }
    if (add_to_root && parent_) {
        scene_->detach_as_root_object(unsafe_make_ref(this));
    }
    parent_ = nullptr;
    next_sibling_ = nullptr;
    prev_sibling_ = nullptr;
}

auto SceneObject::extract_all_children() -> void {
    if (!first_child_) { return; }

    if (parent_) {
        first_child_->prev_sibling_ = this;
        last_child_->next_sibling_ = next_sibling_;
        if (next_sibling_) {
            next_sibling_->prev_sibling_ = last_child_;
        }
        next_sibling_ = first_child_;
        first_child_ = nullptr;
        last_child_ = nullptr;
        for_each_children([this](Ref<SceneObject> object) { object->parent_ = parent_; });
    } else {
        for_each_children([this](Ref<SceneObject> object) {
            object->next_sibling_ = nullptr;
            object->prev_sibling_ = nullptr;
            object->parent_ = nullptr;
            scene_->detach_as_root_object(object);
        });
    }
}

auto SceneObject::clone(bool include_children, Ptr<Scene> dst_scene) const -> Ref<SceneObject> {
    auto clone_object = [](CRef<SceneObject> src, Ref<SceneObject> dst) {
        dst->set_name(src->get_name());
        src->for_each_component([dst](std::string_view component_type_name, void const* value) {
            auto& clone_func = g_engine->component_manager()->get_metadata(component_type_name).clone_to;
            clone_func(dst, value);
        });
    };

    if (!dst_scene) {
        dst_scene = scene_;
    }
    auto dst = dst_scene->create_scene_object(dst_scene == scene_ ? parent_ : nullptr, false);

    std::vector<std::pair<CRef<SceneObject>, Ref<SceneObject>>> stack{};
    stack.emplace_back(unsafe_make_cref(this), dst);
    while (!stack.empty()) {
        auto [u_src, u_dst] = stack.back();
        stack.pop_back();
        clone_object(u_src, u_dst);
        u_src->for_each_children([&stack, &u_dst, dst_scene](CRef<SceneObject> src_ch) {
            auto dst_ch = dst_scene->create_scene_object(u_dst, false);
            stack.emplace_back(src_ch, dst_ch);
        });
    }

    return dst;
}

auto SceneObject::for_each_children(std::function<auto(Ref<SceneObject>) -> void> op) -> void {
    for (auto object = first_child_; object; object = object->next_sibling_) {
        op(object.value());
    }
}
auto SceneObject::for_each_children(std::function<auto(CRef<SceneObject>) -> void> op) const -> void {
    for (auto object = first_child_; object; object = object->next_sibling_) {
        op(object.value());
    }
}

auto SceneObject::for_each_component(std::function<auto(std::string_view, void*) -> void> func) -> void {
    for (auto& [name, value] : components_) {
        func(name, value);
    }
}
auto SceneObject::for_each_component(std::function<auto(std::string_view, void const*) -> void> func) const -> void {
    for (auto& [name, value] : components_) {
        func(name, value);
    }
}

}

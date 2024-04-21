#include <bisemutum/runtime/scene.hpp>

#include <vector>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/runtime/component_manager.hpp>

namespace bi::rt {

auto Scene::ecs_registry() -> entt::registry& { return ecs_registry_; }
auto Scene::ecs_registry() const -> entt::registry const& { return ecs_registry_; }

auto Scene::object_of(entt::entity esc_entity) const -> Ref<SceneObject> {
    return entity_map_.at(esc_entity);
}

auto Scene::for_each_object(std::function<auto(Ref<SceneObject>) -> void> op) -> void {
    std::for_each(objects_.begin(), objects_.end(), std::move(op));
}
auto Scene::for_each_object(std::function<auto(CRef<SceneObject>) -> void> op) const -> void {
    std::for_each(objects_.begin(), objects_.end(), std::move(op));
}

auto Scene::for_each_root_object(std::function<auto(Ref<SceneObject>) -> void> op) -> void {
    std::for_each(root_objects_.begin(), root_objects_.end(), std::move(op));
}
auto Scene::for_each_root_object(std::function<auto(CRef<SceneObject>) -> void> op) const -> void {
    std::for_each(root_objects_.begin(), root_objects_.end(), std::move(op));
}

auto Scene::detach_as_root_object(Ref<SceneObject> object) -> void {
    if (auto it = root_objects_it_map_.find(object.get()); it == root_objects_it_map_.end()) {
        root_objects_.push_back(object);
        auto root_it = root_objects_.end();
        root_objects_it_map_.insert({object.get(), --root_it});
    }
}
auto Scene::attach_as_child_object(Ref<SceneObject> object, Ref<SceneObject> parent) -> void {
    parent->add_child(object);
}
auto Scene::remove_root_object(Ref<SceneObject> object) -> void {
    if (auto it = root_objects_it_map_.find(object.get()); it != root_objects_it_map_.end()) {
        root_objects_.erase(it->second);
    }
}

auto Scene::create_scene_object(Ptr<SceneObject> parent, Transform transform) -> Ref<SceneObject> {
    auto& object = objects_.emplace_back(unsafe_make_ref(this), transform);
    auto it = objects_.end();
    objects_it_map_.insert({&object, --it});
    entity_map_.insert({object.ecs_entity(), object});
    if (parent.has_value()) {
        attach_as_child_object(object, parent.value());
    } else {
        detach_as_root_object(object);
    }
    return object;
}
auto Scene::create_scene_object(Ptr<SceneObject> parent, bool with_transform) -> Ref<SceneObject> {
    auto& object = objects_.emplace_back(unsafe_make_ref(this), with_transform);
    auto it = objects_.end();
    objects_it_map_.insert({&object, --it});
    entity_map_.insert({object.ecs_entity(), object});
    if (parent.has_value()) {
        attach_as_child_object(object, parent.value());
    } else {
        detach_as_root_object(object);
    }
    return object;
}

auto Scene::destroy_scene_object(Ref<SceneObject> object) -> void {
    object->extract_all_children();
    object->remove_self_from_sibling_list(false);
    object->mark_as_destroyed();
    entity_map_.erase(object->ecs_entity());
    destroyed_objects_.push_back(object);
}

auto Scene::destroy_scene_object_and_its_children(Ref<SceneObject> object) -> void {
    object->remove_self_from_sibling_list(false);
    object->mark_as_destroyed();
    destroyed_objects_.push_back(object);
    std::function<auto(Ref<SceneObject>) -> void> destroy_all_children =
        [this, &destroy_all_children](Ref<SceneObject> ch) {
            ch->mark_as_destroyed();
            destroyed_objects_.push_back(ch);
            ch->for_each_children(destroy_all_children);
        };
    object->for_each_children(destroy_all_children);
}

auto Scene::do_destroy_scene_objects() -> void {
    for (auto object : destroyed_objects_) {
        auto it = objects_it_map_.find(object.get());
        objects_.erase(it->second);
        objects_it_map_.erase(it);
    }
    destroyed_objects_.clear();
}

auto Scene::load_from_value(serde::Value &&value) -> void {
    if (!value.contains("objects")) { return; }

    auto& scene_objects_value = value["objects"].get_ref<serde::Value::Array>();
    std::vector<Ref<SceneObject>> parsed_objects;
    parsed_objects.reserve(scene_objects_value.size());
    std::vector<int> objects_parent;
    objects_parent.reserve(scene_objects_value.size());
    for (auto& object_value : scene_objects_value) {
        auto& object_table = object_value.get_ref<serde::Value::Table>();
        int parent = -1;
        if (auto it = object_table.find("parent"); it != object_table.end()) {
            parent = it->second.get<int>();
        }
        objects_parent.push_back(parent);
        parsed_objects.push_back(create_scene_object(nullptr, false));
        if (auto it = object_table.find("components"); it != object_table.end()) {
            for (auto& component_value : it->second.get_ref<serde::Value::Array>()) {
                auto deserializer = g_engine->component_manager()->get_deserializer(
                    component_value["type"].get_ref<serde::Value::String>()
                );
                deserializer(parsed_objects.back(), component_value["value"]);
            }
        }
        if (auto it = object_table.find("name"); it != object_table.end()) {
            parsed_objects.back()->set_name(it->second.get_ref<serde::Value::String>());
        }
    }
    for (size_t i = 0; i < parsed_objects.size(); i++) {
        if (objects_parent[i] >= 0 && objects_parent[i] < parsed_objects.size()) {
            parsed_objects[i]->attach_under(parsed_objects[objects_parent[i]]);
        }
    }
}
auto Scene::save_to_value(serde::Value& value) const -> void {
    auto& objects_value = value["objects"];
    std::vector<std::pair<CRef<SceneObject>, size_t>> stack{};
    for_each_root_object([&objects_value, &stack](CRef<SceneObject> object) {
        serde::Value object_value{};
        object_value["name"] = std::string{object->get_name()};
        auto& components_value = object_value["components"];
        object->for_each_component([&components_value](std::string_view component_type, void const* component_value) {
            auto serializer = g_engine->component_manager()->get_serializer(component_type);
            serde::Value value{};
            serializer(value["value"], component_value);
            value["type"] = serde::Value::String{component_type};
            components_value.push_back(std::move(value));
        });
        stack.emplace_back(object, stack.size());
        objects_value.push_back(std::move(object_value));
    });
    while (!stack.empty()) {
        auto [u, index] = stack.back();
        stack.pop_back();
        u->for_each_children([&objects_value, &stack, index](CRef<SceneObject> ch) {
            serde::Value object_value{};
            object_value["name"] = std::string{ch->get_name()};
            object_value["parent"] = static_cast<serde::Value::Integer>(index);
            auto& components_value = object_value["components"];
            ch->for_each_component([&components_value](std::string_view component_type, void const* component_value) {
                auto serializer = g_engine->component_manager()->get_serializer(component_type);
                serde::Value value{};
                serializer(value["value"], component_value);
                value["type"] = serde::Value::String{component_type};
                components_value.push_back(std::move(value));
            });
            objects_value.push_back(std::move(object_value));
            stack.emplace_back(ch, objects_value.size());
        });
    }
}

}

#include <bisemutum/runtime/scene.hpp>

#include <vector>

#include <bisemutum/runtime/scene_object.hpp>

namespace bi::rt {

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

auto Scene::add_root_object(Ref<SceneObject> object) -> void {
    if (auto it = objects_it_map_.find(object.get()); it == objects_it_map_.end()) {
        objects_.push_front(object);
        objects_it_map_.insert({object.get(), objects_.begin()});
    }
    if (auto it = root_objects_it_map_.find(object.get()); it == root_objects_it_map_.end()) {
        root_objects_.push_front(object);
        root_objects_it_map_.insert({object.get(), root_objects_.begin()});
    }
}
auto Scene::add_child_object(Ref<SceneObject> object, Ref<SceneObject> parent) -> void {
    if (auto it = objects_it_map_.find(object.get()); it == objects_it_map_.end()) {
        objects_.push_front(object);
        objects_it_map_.insert({object.get(), objects_.begin()});
    }
    parent->add_child(object);
}
auto Scene::remove_object(Ref<SceneObject> object) -> void {
    if (auto it = objects_it_map_.find(object.get()); it != objects_it_map_.end()) {
        objects_.erase(it->second);
    }
    if (auto it = root_objects_it_map_.find(object.get()); it != root_objects_it_map_.end()) {
        root_objects_.erase(it->second);
    }
}
auto Scene::remove_root_object(Ref<SceneObject> object) -> void {
    if (auto it = root_objects_it_map_.find(object.get()); it != root_objects_it_map_.end()) {
        root_objects_.erase(it->second);
    }
}

}

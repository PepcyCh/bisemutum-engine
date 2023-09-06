#pragma once

#include <list>
#include <unordered_map>
#include <functional>

#include "../prelude/ref.hpp"

namespace bi::rt {

struct SceneObject;

struct Scene final {
    auto for_each_object(std::function<auto(Ref<SceneObject>) -> void> op) -> void;
    auto for_each_object(std::function<auto(CRef<SceneObject>) -> void> op) const -> void;

    auto for_each_root_object(std::function<auto(Ref<SceneObject>) -> void> op) -> void;
    auto for_each_root_object(std::function<auto(CRef<SceneObject>) -> void> op) const -> void;

    auto add_root_object(Ref<SceneObject> object) -> void;
    auto add_child_object(Ref<SceneObject> object, Ref<SceneObject> parent) -> void;
    auto remove_object(Ref<SceneObject> object) -> void;
    auto remove_root_object(Ref<SceneObject> object) -> void;

private:
    std::list<Ref<SceneObject>> objects_;
    std::list<Ref<SceneObject>> root_objects_;
    std::unordered_map<SceneObject*, std::list<Ref<SceneObject>>::iterator> objects_it_map_;
    std::unordered_map<SceneObject*, std::list<Ref<SceneObject>>::iterator> root_objects_it_map_;
};

}

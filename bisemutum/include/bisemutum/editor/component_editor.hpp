#pragma once

#include "basic.hpp"
#include "../runtime/component.hpp"
#include "../runtime/scene_object.hpp"

namespace bi::editor {

template <rt::TComponent Component>
auto default_component_editor(Ref<rt::SceneObject> object, Component* value) -> bool {
    auto dirty = edit(*value);
    if (dirty) {
        object->notify_component_dirty<Component>();
    }
    return dirty;
}

}

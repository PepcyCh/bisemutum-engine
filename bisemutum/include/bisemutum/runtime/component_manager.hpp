#pragma once

#include "component.hpp"
#include "scene_object.hpp"
#include "../prelude/idiom.hpp"

namespace bi::rt {

using ComponentDeserializer = auto(Ref<SceneObject>, serde::Value const&) -> void;

struct ComponentManager final : PImpl<ComponentManager> {
    struct Impl;

    ComponentManager();
    ~ComponentManager();

    template <TComponent Component>
    auto register_component() {
        register_component(
            Component::component_type_name,
            [](Ref<SceneObject> object, serde::Value const& component_value) {
                object->attach_component(component_value.get<Component>());
            }
        );
    }

    auto get_deserializer(std::string_view type) const -> std::function<ComponentDeserializer> const&;

private:
    auto register_component(
        std::string_view type,
        std::function<ComponentDeserializer> deserializer
    ) -> void;
};

}

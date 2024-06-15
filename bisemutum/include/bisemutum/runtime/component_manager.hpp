#pragma once

#include <typeindex>

#include "component.hpp"
#include "scene_object.hpp"
#include "../prelude/idiom.hpp"
#include "../editor/component_editor.hpp"

namespace bi::rt {

using ComponentDeserializer = auto(Ref<SceneObject>, serde::Value const&) -> void;
using ComponentSerializer = auto(serde::Value&, void const*) -> void;
using ComponentEditor = auto(Ref<SceneObject>, void*) -> bool;
using ComponentAttach = auto(Ref<SceneObject>) -> void;
using ComponentClone = auto(Ref<SceneObject>, void const*) -> void;

struct ComponentManager final : PImpl<ComponentManager> {
    struct Impl;

    ComponentManager();

    struct ComponentMetadata final {
        std::type_index type_index;
        std::function<ComponentDeserializer> deserializer;
        std::function<ComponentSerializer> serializer;
        std::function<ComponentEditor> editor;
        std::function<ComponentAttach> attach;
        std::function<ComponentClone> clone_to;
    };

    template <TComponent Component>
    auto register_component() {
        ComponentMetadata metadata{
            .type_index = typeid(Component),
            .deserializer = [](Ref<SceneObject> object, serde::Value const& component_value) {
                object->attach_component(component_value.get<Component>());
            },
            .serializer = [](serde::Value& serde_value, void const* component_value) {
                serde::to_value(serde_value, *reinterpret_cast<Component const*>(component_value));
            },
            .editor = [](Ref<SceneObject> object, void* component_value) -> bool {
                return editor::default_component_editor(object, reinterpret_cast<Component*>(component_value));
            },
            .attach = [](Ref<SceneObject> object) {
                object->attach_component(Component{});
            },
            .clone_to = [](Ref<SceneObject> object, void const* component_value) {
                auto component = *reinterpret_cast<Component const*>(component_value);
                object->attach_component(std::move(component));
            },
        };
        register_component(Component::component_type_name, std::move(metadata));
    }

    auto get_metadata(std::string_view type) const -> ComponentMetadata const&;
    auto try_get_metadata(std::string_view type) const -> ComponentMetadata const*;

    auto get_deserializer(std::string_view type) const -> std::function<ComponentDeserializer> const&;
    auto get_serializer(std::string_view type) const -> std::function<ComponentSerializer> const&;
    auto get_editor(std::string_view type) const -> std::function<ComponentEditor> const&;

private:
    auto register_component(
        std::string_view type,
        ComponentMetadata&& metadata
    ) -> void;
};

}

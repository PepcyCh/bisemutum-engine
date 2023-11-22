#pragma once

#include <typeindex>

#include "component.hpp"
#include "scene_object.hpp"
#include "../prelude/idiom.hpp"
#include "../editor/component_editor.hpp"

namespace bi::rt {

using ComponentDeserializer = auto(Ref<SceneObject>, serde::Value const&) -> void;
using ComponentEditor = auto(Ref<SceneObject>, void*) -> bool;
using ComponentClone = auto(Ref<SceneObject>, void*) -> void;

struct ComponentManager final : PImpl<ComponentManager> {
    struct Impl;

    ComponentManager();
    ~ComponentManager();

    struct ComponentMetadata final {
        std::type_index type_index;
        std::function<ComponentDeserializer> deserializer;
        std::function<ComponentEditor> editor;
        std::function<ComponentClone> clone_to;
    };

    template <TComponent Component>
    auto register_component() {
        ComponentMetadata metadata{
            .type_index = typeid(Component),
            .deserializer = [](Ref<SceneObject> object, serde::Value const& component_value) {
                object->attach_component(component_value.get<Component>());
            },
            .clone_to = [](Ref<SceneObject> object, void* component_value) {
                auto component = *reinterpret_cast<Component*>(component_value);
                object->attach_component(std::move(component));
            },
        };
        if constexpr (
            requires(Ref<SceneObject> object, Component* component_value) {
                { Component::editor(object, component_value) } -> std::convertible_to<bool>;
            }
        ) {
            metadata.editor = [](Ref<SceneObject> object, void* component_value) -> bool {
                return Component::editor(object, reinterpret_cast<Component*>(component_value));
            };
        } else {
            metadata.editor = [](Ref<SceneObject> object, void* component_value) -> bool {
                return editor::default_component_editor(object, reinterpret_cast<Component*>(component_value));
            };
        }
        register_component(Component::component_type_name, std::move(metadata));
    }

    auto get_metadata(std::string_view type) const -> ComponentMetadata const&;
    auto get_deserializer(std::string_view type) const -> std::function<ComponentDeserializer> const&;
    auto get_editor(std::string_view type) const -> std::function<ComponentEditor> const&;

private:
    auto register_component(
        std::string_view type,
        ComponentMetadata&& metadata
    ) -> void;
};

}

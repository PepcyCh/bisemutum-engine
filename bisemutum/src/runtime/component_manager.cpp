#include <bisemutum/runtime/component_manager.hpp>

namespace bi::rt {

struct ComponentManager::Impl final {
    auto register_component(
        std::string_view type,
        ComponentMetadata&& metadata
    ) -> void {
        components_metadata.insert({type, std::move(metadata)});
    }

    auto get_metadata(std::string_view type) const -> ComponentMetadata const& {
        return components_metadata.at(type);
    }

    std::unordered_map<std::string_view, ComponentMetadata> components_metadata;
};

ComponentManager::ComponentManager() = default;
ComponentManager::~ComponentManager() = default;

auto ComponentManager::register_component(
    std::string_view type,
    ComponentMetadata&& metadata
) -> void {
    impl()->register_component(type, std::move(metadata));
}

auto ComponentManager::get_metadata(std::string_view type) const -> ComponentMetadata const& {
    return impl()->get_metadata(type);
}

auto ComponentManager::get_deserializer(std::string_view type) const -> std::function<ComponentDeserializer> const& {
    return get_metadata(type).deserializer;
}
auto ComponentManager::get_editor(std::string_view type) const -> std::function<ComponentEditor> const& {
    return get_metadata(type).editor;
}

}

#include <bisemutum/runtime/component_manager.hpp>

namespace bi::rt {

struct ComponentManager::Impl final {
    auto register_component(
        std::string_view type,
        std::function<ComponentDeserializer>&& deserializer
    ) -> void {
        deserializers.insert({type, std::move(deserializer)});
    }

    auto get_deserializer(std::string_view type) const -> std::function<ComponentDeserializer> const& {
        return deserializers.at(type);
    }

    std::unordered_map<std::string_view, std::function<ComponentDeserializer>> deserializers;
};

ComponentManager::ComponentManager() = default;
ComponentManager::~ComponentManager() = default;

auto ComponentManager::register_component(
    std::string_view type,
    std::function<ComponentDeserializer> deserializer
) -> void {
    impl()->register_component(type, std::move(deserializer));
}

auto ComponentManager::get_deserializer(std::string_view type) const -> std::function<ComponentDeserializer> const& {
    return impl()->get_deserializer(type);
}

}

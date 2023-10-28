#include <bisemutum/utils/drefl.hpp>

namespace bi::drefl {

struct ReflectionManager::Impl final {
    auto register_type(std::type_index type, TypeInfo&& info) -> void {
        type_infos.push_front(std::move(info));
        auto it = type_infos.begin();
        map_type.insert({type, it});
        map_name.insert({it->name, it});
    }

    auto get_type_info(std::type_index type) const -> TypeInfo const& {
        return *map_type.find(type)->second;
    }
    auto get_type_info(std::string_view name) const -> TypeInfo const& {
        return *map_name.find(name)->second;
    }

    std::list<TypeInfo> type_infos;
    std::unordered_map<std::type_index, decltype(type_infos)::iterator> map_type;
    std::unordered_map<std::string_view, decltype(type_infos)::iterator> map_name;
};

ReflectionManager::ReflectionManager() = default;
ReflectionManager::~ReflectionManager() = default;

auto ReflectionManager::register_type(std::type_index type, TypeInfo&& info) -> void {
    impl()->register_type(type, std::move(info));
}

auto ReflectionManager::get_type_info(std::type_index type) const -> TypeInfo const& {
    return impl()->get_type_info(type);
}
auto ReflectionManager::get_type_info(std::string_view name) const -> TypeInfo const& {
    return impl()->get_type_info(name);
}

}

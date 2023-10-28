#pragma once

#include <typeindex>
#include <functional>
#include <unordered_map>

#include "srefl.hpp"
#include "../prelude/poly.hpp"
#include "../prelude/idiom.hpp"

namespace bi::drefl {

using Value = aa::any_with<aa::move>;

struct FieldInfo final {
    std::string_view name;
    std::function<auto(void const*) -> Value> getter;
    std::function<auto(void*, void const*) -> void> setter;
};

struct TypeInfo final {
    std::string_view namespace_name;
    std::string_view name;
    std::unordered_map<std::string_view, FieldInfo> fields;
};

struct ReflectionManager final : PImpl<ReflectionManager> {
    struct Impl;

    ReflectionManager();
    ~ReflectionManager();

    template <typename T>
    auto register_type() -> void {
        constexpr auto sinfo = srefl::refl<T>();
        TypeInfo dinfo{
            .namespace_name = sinfo.namespace_name,
            .name = sinfo.type_name,
        };
        srefl::for_each(sinfo.members, [&dinfo](auto member) {
            dinfo.fields.insert({
                member.name,
                FieldInfo{
                    .name = member.name,
                    .getter = [&member](void const* object) -> Value {
                        return Value{member(*reinterpret_cast<T const*>(object))};
                    },
                    .setter = [&member](void* object, void const* value) -> void {
                        member(*reinterpret_cast<T*>(object)) =
                            *reinterpret_cast<typename decltype(member)::FieldType const*>(value);
                    },
                },
            });
        });
        register_type(typeid(T), std::move(dinfo));
    }

    auto get_type_info(std::type_index type) const -> TypeInfo const&;
    auto get_type_info(std::string_view name) const -> TypeInfo const&;

private:
    auto register_type(std::type_index type, TypeInfo&& info) -> void;
};

}

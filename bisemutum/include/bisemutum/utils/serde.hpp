#pragma once

#include <nlohmann/json.hpp>

#include "../macros/map.hpp"

namespace bi {

namespace json = nlohmann;

template <typename T>
concept JsonSerdeable = requires () {
    { std::declval<json::json>().get<T>() } -> std::same_as<T>;
    std::convertible_to<T, json::json>;
};

#define BI_MACRO_MAP_OP_JSON_SERDE_ENUM_ITEM(f, x) {f::x, #x},
#define BI_JSON_SERDE_ENUM(ty, ...) NLOHMANN_JSON_SERIALIZE_ENUM( \
    ty, { BI_MACRO_MAP_OP(BI_MACRO_MAP_OP_JSON_SERDE_ENUM_ITEM, ty, __VA_ARGS__) } \
)

}

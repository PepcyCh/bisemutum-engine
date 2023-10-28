#pragma once

#include <imgui.h>

#include "../utils/srefl.hpp"
#include "../math/math.hpp"
#include "../prelude/traits.hpp"

namespace bi::editor {

template <traits::Enum E>
auto edit_enum(std::string_view name, E& value) -> bool {
    constexpr auto values = magic_enum::enum_values<E>();
    std::array<char const*, values.size()> names{};
    for (size_t i = 0; i < values.size(); i++) {
        names[i] = magic_enum::enum_name<E>(magic_enum::enum_value<E>(i)).data();
    }
    auto curr = static_cast<int>(magic_enum::enum_index<E>(value).value());
    auto dirty = ImGui::Combo(name.data(), &curr, names.data(), names.size());
    value = magic_enum::enum_value<E>(curr);
    return dirty;
}

auto edit_float(std::string_view name, float& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f) -> bool;
auto edit_float2(std::string_view name, float2& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f) -> bool;
auto edit_float3(std::string_view name, float3& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f) -> bool;
auto edit_float4(std::string_view name, float4& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f) -> bool;

auto edit_int(std::string_view name, int& value, float speed = 1.0f, int min = 0, int max = 0) -> bool;
auto edit_int2(std::string_view name, int2& value, float speed = 1.0f, int min = 0, int max = 0) -> bool;
auto edit_int3(std::string_view name, int3& value, float speed = 1.0f, int min = 0, int max = 0) -> bool;
auto edit_int4(std::string_view name, int4& value, float speed = 1.0f, int min = 0, int max = 0) -> bool;

auto edit_uint(std::string_view name, uint32_t& value, float speed = 1.0f, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint2(std::string_view name, uint2& value, float speed = 1.0f, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint3(std::string_view name, uint3& value, float speed = 1.0f, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint4(std::string_view name, uint4& value, float speed = 1.0f, uint32_t min = 0, uint32_t max = 0) -> bool;

auto edit_bool(std::string_view name, bool& value) -> bool;

}

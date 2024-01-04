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
auto edit_float2_range(std::string_view name, float2& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f) -> bool;

auto edit_color3(std::string_view name, float3& value) -> bool;
auto edit_color4(std::string_view name, float4& value) -> bool;

auto edit_int(std::string_view name, int& value, int speed = 1, int min = 0, int max = 0) -> bool;
auto edit_int2(std::string_view name, int2& value, int speed = 1, int min = 0, int max = 0) -> bool;
auto edit_int3(std::string_view name, int3& value, int speed = 1, int min = 0, int max = 0) -> bool;
auto edit_int4(std::string_view name, int4& value, int speed = 1, int min = 0, int max = 0) -> bool;
auto edit_int2_range(std::string_view name, int2& value, int speed = 1, int min = 0, int max = 0) -> bool;

auto edit_uint(std::string_view name, uint32_t& value, uint32_t speed = 1, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint2(std::string_view name, uint2& value, uint32_t speed = 1, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint3(std::string_view name, uint3& value, uint32_t speed = 1, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint4(std::string_view name, uint4& value, uint32_t speed = 1, uint32_t min = 0, uint32_t max = 0) -> bool;
auto edit_uint2_range(std::string_view name, uint2& value, uint32_t speed = 1, uint32_t min = 0, uint32_t max = 0) -> bool;

auto edit_bool(std::string_view name, bool& value) -> bool;


template <typename T>
auto edit(T& o) -> bool {
    if constexpr (requires { o.edit(); }) {
        return o.edit();
    } else if constexpr (srefl::is_reflectable<T>) {
        constexpr auto info = srefl::refl<T>();
        auto edited = false;
        srefl::for_each(info.members, [&o, &edited](auto member) {
            if constexpr (std::is_same_v<typename decltype(member)::FieldType, float>) {
                auto range = member.template get_attribute(RangeF{0.0f, 0.0f, 0.01f});
                edited |= edit_float(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, float2>) {
                auto range = member.template get_attribute(RangeF{0.0f, 0.0f, 0.01f});
                if constexpr (member.template has_attribute<MinMaxRange>()) {
                    edited |= edit_float2_range(member.name, member(o), range.step, range.min, range.max);
                } else {
                    edited |= edit_float2(member.name, member(o), range.step, range.min, range.max);
                }
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, float3>) {
                if constexpr (member.template has_attribute<Color>()) {
                    edited |= edit_color3(member.name, member(o));
                } else {
                    auto range = member.template get_attribute(RangeF{0.0f, 0.0f, 0.01f});
                    edited |= edit_float3(member.name, member(o), range.step, range.min, range.max);
                }
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, float4>) {
                if constexpr (member.template has_attribute<Color>()) {
                    edited |= edit_color4(member.name, member(o));
                } else {
                    auto range = member.template get_attribute(RangeF{0.0f, 0.0f, 0.01f});
                    edited |= edit_float4(member.name, member(o), range.step, range.min, range.max);
                }
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, int>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                edited |= edit_int(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, int2>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                if constexpr (member.template has_attribute<MinMaxRange>()) {
                    edited |= edit_int2_range(member.name, member(o), range.step, range.min, range.max);
                } else {
                    edited |= edit_int2(member.name, member(o), range.step, range.min, range.max);
                }
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, int3>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                edited |= edit_int3(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, int4>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                edited |= edit_int4(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, uint32_t>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                edited |= edit_uint(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, uint2>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                if constexpr (member.template has_attribute<MinMaxRange>()) {
                    edited |= edit_uint2_range(member.name, member(o), range.step, range.min, range.max);
                } else {
                    edited |= edit_uint2(member.name, member(o), range.step, range.min, range.max);
                }
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, uint3>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                edited |= edit_uint3(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, uint4>) {
                auto range = member.template get_attribute(RangeI{0, 0, 1});
                edited |= edit_uint4(member.name, member(o), range.step, range.min, range.max);
            } else if constexpr (std::is_same_v<typename decltype(member)::FieldType, bool>) {
                edited |= edit_bool(member.name, member(o));
            } else if constexpr (std::is_enum_v<typename decltype(member)::FieldType>) {
                edited |= edit_enum(member.name, member(o));
            } else {
                if (ImGui::TreeNode(member.name.data())) {
                    edited |= edit(member(o));
                    ImGui::TreePop();
                }
            }
        });
        return edited;
    } else {
        ImGui::Text("(unsupported)");
        return false;
    }
}

}

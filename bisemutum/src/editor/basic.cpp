#include <bisemutum/editor/basic.hpp>

namespace bi::editor {

auto edit_float(std::string_view name, float& value, float speed, float min, float max) -> bool {
    return ImGui::DragFloat(name.data(), &value, speed, min, max);
}
auto edit_float2(std::string_view name, float2& value, float speed, float min, float max) -> bool {
    return ImGui::DragFloat2(name.data(), &value.x, speed, min, max);
}
auto edit_float3(std::string_view name, float3& value, float speed, float min, float max) -> bool {
    return ImGui::DragFloat3(name.data(), &value.x, speed, min, max);
}
auto edit_float4(std::string_view name, float4& value, float speed, float min, float max) -> bool {
    return ImGui::DragFloat4(name.data(), &value.x, speed, min, max);
}

auto edit_color3(std::string_view name, float3& value) -> bool {
    return ImGui::ColorEdit3(name.data(), &value.x);
}
auto edit_color4(std::string_view name, float4& value) -> bool {
    return ImGui::ColorEdit4(name.data(), &value.x);
}

auto edit_int(std::string_view name, int& value, int speed, int min, int max) -> bool {
    return ImGui::DragInt(name.data(), &value, speed, max, max);
}
auto edit_int2(std::string_view name, int2& value, int speed, int min, int max) -> bool {
    return ImGui::DragInt2(name.data(), &value.x, speed, max, max);
}
auto edit_int3(std::string_view name, int3& value, int speed, int min, int max) -> bool {
    return ImGui::DragInt3(name.data(), &value.x, speed, max, max);
}
auto edit_int4(std::string_view name, int4& value, int speed, int min, int max) -> bool {
    return ImGui::DragInt4(name.data(), &value.x, speed, max, max);
}

auto edit_uint(std::string_view name, uint32_t& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = static_cast<int>(value);
    auto dirty = ImGui::DragInt(name.data(), &int_value, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint2(std::string_view name, uint2& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int2{value};
    auto dirty = ImGui::DragInt2(name.data(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint3(std::string_view name, uint3& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int3{value};
    auto dirty = ImGui::DragInt3(name.data(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint4(std::string_view name, uint4& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int4{value};
    auto dirty = ImGui::DragInt4(name.data(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}

auto edit_bool(std::string_view name, bool& value) -> bool {
    return ImGui::Checkbox(name.data(), &value);
}

}

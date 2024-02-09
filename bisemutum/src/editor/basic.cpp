#include <bisemutum/editor/basic.hpp>

namespace bi::editor {

namespace {

auto imgui_label(std::string_view name, void const* data) -> std::string {
    return fmt::format("{}##{}", name, reinterpret_cast<size_t>(data));
}

} // namespace

auto edit_float(std::string_view name, float& value, float speed, float min, float max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragFloat(label.c_str(), &value, speed, min, max);
}
auto edit_float2(std::string_view name, float2& value, float speed, float min, float max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragFloat2(label.c_str(), &value.x, speed, min, max);
}
auto edit_float3(std::string_view name, float3& value, float speed, float min, float max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragFloat3(label.c_str(), &value.x, speed, min, max);
}
auto edit_float4(std::string_view name, float4& value, float speed, float min, float max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragFloat4(label.c_str(), &value.x, speed, min, max);
}
auto edit_float2_range(std::string_view name, float2& value, float speed, float min, float max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragFloatRange2(label.c_str(), &value.x, &value.y, speed, min, max);
}

auto edit_color3(std::string_view name, float3& value) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::ColorEdit3(label.c_str(), &value.x);
}
auto edit_color4(std::string_view name, float4& value) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::ColorEdit4(label.c_str(), &value.x);
}

auto edit_int(std::string_view name, int& value, int speed, int min, int max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragInt(label.c_str(), &value, speed, max, max);
}
auto edit_int2(std::string_view name, int2& value, int speed, int min, int max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragInt2(label.c_str(), &value.x, speed, max, max);
}
auto edit_int3(std::string_view name, int3& value, int speed, int min, int max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragInt3(label.c_str(), &value.x, speed, max, max);
}
auto edit_int4(std::string_view name, int4& value, int speed, int min, int max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragInt4(label.c_str(), &value.x, speed, max, max);
}
auto edit_int2_range(std::string_view name, int2& value, int speed, int min, int max) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::DragIntRange2(label.c_str(), &value.x, &value.y, speed, min, max);
}

auto edit_uint(std::string_view name, uint32_t& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = static_cast<int>(value);
    auto label = imgui_label(name, &value);
    auto dirty = ImGui::DragInt(label.c_str(), &int_value, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint2(std::string_view name, uint2& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int2{value};
    auto label = imgui_label(name, &value);
    auto dirty = ImGui::DragInt2(label.c_str(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint3(std::string_view name, uint3& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int3{value};
    auto label = imgui_label(name, &value);
    auto dirty = ImGui::DragInt3(label.c_str(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint4(std::string_view name, uint4& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int4{value};
    auto label = imgui_label(name, &value);
    auto dirty = ImGui::DragInt4(label.c_str(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint2_range(std::string_view name, uint2& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int2{value};
    auto label = imgui_label(name, &value);
    auto dirty = ImGui::DragIntRange2(label.c_str(), &int_value.x, &int_value.y, speed, min, max);
    value = int_value;
    return dirty;
}

auto edit_bool(std::string_view name, bool& value) -> bool {
    auto label = imgui_label(name, &value);
    return ImGui::Checkbox(label.c_str(), &value);
}

}

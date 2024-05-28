#include <bisemutum/editor/basic.hpp>

namespace bi::editor {

auto label(std::string_view name, void const* p_data) -> std::string {
    auto label = std::string{name};
    for (auto& ch : label) {
        if (ch == '_') {
            ch = ' ';
        }
    }

    auto cursor_x = ImGui::GetCursorPosX();
    auto full_width = ImGui::GetContentRegionAvail().x;
    auto item_width = ImGui::CalcItemWidth() * 0.8f + ImGui::GetStyle().ItemInnerSpacing.x;
    auto max_text_width = full_width - item_width;

    auto label_end = label.c_str() + label.size();
    auto label_end_l = label.c_str();
    while (label_end_l < label_end) {
        auto mid = label_end_l + (label_end - label_end_l) / 2 + 1;
        auto label_width = ImGui::CalcTextSize(label.c_str(), mid).x;
        if (label_width > max_text_width) {
            label_end = mid - 1;
        } else {
            label_end_l = mid;
        }
    }
    ImGui::TextUnformatted(label.c_str(), label_end);
    ImGui::SetItemTooltip("%s", label.c_str());
	ImGui::SameLine();
    ImGui::SetCursorPosX(cursor_x + max_text_width);
    ImGui::SetNextItemWidth(item_width);

    return fmt::format("##{}-{}", name, reinterpret_cast<size_t>(p_data));
}

auto edit_float(std::string_view name, float& value, float speed, float min, float max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragFloat(lbl.c_str(), &value, speed, min, max);
}
auto edit_float2(std::string_view name, float2& value, float speed, float min, float max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragFloat2(lbl.c_str(), &value.x, speed, min, max);
}
auto edit_float3(std::string_view name, float3& value, float speed, float min, float max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragFloat3(lbl.c_str(), &value.x, speed, min, max);
}
auto edit_float4(std::string_view name, float4& value, float speed, float min, float max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragFloat4(lbl.c_str(), &value.x, speed, min, max);
}
auto edit_float2_range(std::string_view name, float2& value, float speed, float min, float max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragFloatRange2(lbl.c_str(), &value.x, &value.y, speed, min, max);
}

auto edit_color3(std::string_view name, float3& value) -> bool {
    auto lbl = label(name, &value);
    return ImGui::ColorEdit3(lbl.c_str(), &value.x);
}
auto edit_color4(std::string_view name, float4& value) -> bool {
    auto lbl = label(name, &value);
    return ImGui::ColorEdit4(lbl.c_str(), &value.x);
}

auto edit_int(std::string_view name, int& value, int speed, int min, int max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragInt(lbl.c_str(), &value, speed, max, max);
}
auto edit_int2(std::string_view name, int2& value, int speed, int min, int max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragInt2(lbl.c_str(), &value.x, speed, max, max);
}
auto edit_int3(std::string_view name, int3& value, int speed, int min, int max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragInt3(lbl.c_str(), &value.x, speed, max, max);
}
auto edit_int4(std::string_view name, int4& value, int speed, int min, int max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragInt4(lbl.c_str(), &value.x, speed, max, max);
}
auto edit_int2_range(std::string_view name, int2& value, int speed, int min, int max) -> bool {
    auto lbl = label(name, &value);
    return ImGui::DragIntRange2(lbl.c_str(), &value.x, &value.y, speed, min, max);
}

auto edit_uint(std::string_view name, uint32_t& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = static_cast<int>(value);
    auto lbl = label(name, &value);
    auto dirty = ImGui::DragInt(lbl.c_str(), &int_value, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint2(std::string_view name, uint2& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int2{value};
    auto lbl = label(name, &value);
    auto dirty = ImGui::DragInt2(lbl.c_str(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint3(std::string_view name, uint3& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int3{value};
    auto lbl = label(name, &value);
    auto dirty = ImGui::DragInt3(lbl.c_str(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint4(std::string_view name, uint4& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int4{value};
    auto lbl = label(name, &value);
    auto dirty = ImGui::DragInt4(lbl.c_str(), &int_value.x, speed, min, max);
    value = int_value;
    return dirty;
}
auto edit_uint2_range(std::string_view name, uint2& value, uint32_t speed, uint32_t min, uint32_t max) -> bool {
    auto int_value = int2{value};
    auto lbl = label(name, &value);
    auto dirty = ImGui::DragIntRange2(lbl.c_str(), &int_value.x, &int_value.y, speed, min, max);
    value = int_value;
    return dirty;
}

auto edit_bool(std::string_view name, bool& value) -> bool {
    auto lbl = label(name, &value);
    return ImGui::Checkbox(lbl.c_str(), &value);
}

}

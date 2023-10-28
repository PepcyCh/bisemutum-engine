#pragma once

#include "basic.hpp"
#include "../runtime/component.hpp"
#include "../runtime/scene_object.hpp"

namespace bi::editor {

template <rt::TComponent Component>
auto default_component_editor(Ref<rt::SceneObject> object, Component* value) -> bool {
    auto info = srefl::refl<Component>();
    auto dirty = false;
    srefl::for_each(info.members, [&dirty, value](auto member) {
        using MemberType = typename decltype(member)::FieldType;
        if constexpr (std::is_enum_v<MemberType>) {
            dirty = edit_enum(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, float>) {
            dirty = edit_float(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, float2>) {
            dirty = edit_float2(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, float3>) {
            dirty = edit_float3(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, float4>) {
            dirty = edit_float4(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, int>) {
            dirty = edit_int(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, int2>) {
            dirty = edit_int2(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, int3>) {
            dirty = edit_int3(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, int4>) {
            dirty = edit_int4(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, uint32_t>) {
            dirty = edit_uint(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, uint2>) {
            dirty = edit_uint2(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, uint3>) {
            dirty = edit_uint3(member.name, member(*value));
        } else if constexpr (std::is_same_v<MemberType, uint4>) {
            dirty = edit_uint4(member.name, member(*value));
        } else {
            ImGui::Text("(unsupported) %s", member.name.data());
        }
    });
    if (dirty) {
        object->notify_component_dirty<Component>();
    }
    return dirty;
}

}

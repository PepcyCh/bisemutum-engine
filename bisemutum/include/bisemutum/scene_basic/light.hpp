#pragma once

#include "../math/math_serde.hpp"

namespace bi {

enum class LightType : uint8_t {
    directional,
    point,
    spot,
};

struct LightComponent final {
    static constexpr std::string_view component_type_name = "LightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;
    LightType type = LightType::directional;
    float range = 1e9f;
    float spot_inner_angle = 30.0f;
    float spot_outer_angle = 60.0f;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
};
BI_SREFL(
    type(LightComponent),
    field(color),
    field(strength),
    field(type),
    field(range),
    field(spot_inner_angle),
    field(spot_outer_angle),
    field(cast_shadow),
    field(shadow_strength)
)

}

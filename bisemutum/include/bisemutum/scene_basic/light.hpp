#pragma once

#include "../math/math.hpp"

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

struct DirectionalLightComponent final {
    static constexpr std::string_view component_type_name = "DirectionalLightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
    float shadow_bias_factor = 0.001f;
    float3 cascade_shadow_ratio = float3{0.1f, 0.25f, 0.5f};
};
BI_SREFL(
    type(DirectionalLightComponent),
    field(color),
    field(strength),
    field(cast_shadow),
    field(shadow_strength),
    field(shadow_bias_factor),
    field(cascade_shadow_ratio)
)

struct PointLightComponent final {
    static constexpr std::string_view component_type_name = "PointLightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
};
BI_SREFL(
    type(PointLightComponent),
    field(color),
    field(strength),
    field(cast_shadow),
    field(shadow_strength)
)

struct SpotLightComponent final {
    static constexpr std::string_view component_type_name = "SpotLightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;

    float spot_inner_angle = 30.0f;
    float spot_outer_angle = 60.0f;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
};
BI_SREFL(
    type(SpotLightComponent),
    field(color),
    field(strength),
    field(spot_inner_angle),
    field(spot_outer_angle),
    field(cast_shadow),
    field(shadow_strength)
)

}

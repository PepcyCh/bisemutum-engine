#pragma once

#include "texture.hpp"
#include "../math/math.hpp"
#include "../runtime/asset.hpp"

namespace bi {

struct DirectionalLightComponent final {
    static constexpr std::string_view component_type_name = "DirectionalLightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
    float shadow_range = 100.0f;
    float shadow_depth_bias_factor = 0.002f;
    float shadow_normal_bias_factor = 0.5f;
    float3 cascade_shadow_ratio = float3{0.1f, 0.25f, 0.5f};
};
BI_SREFL(
    type(DirectionalLightComponent),
    field(color, Color{}),
    field(strength, RangeF{}),
    field(cast_shadow),
    field(shadow_strength, RangeF{0.0f, 1.0f}),
    field(shadow_range, RangeF{}),
    field(shadow_depth_bias_factor, RangeF{0.0f, 1.0f}),
    field(shadow_normal_bias_factor, RangeF{0.0f, 1.0f}),
    field(cascade_shadow_ratio, RangeF{0.0f, 1.0f})
)

struct PointLightComponent final {
    static constexpr std::string_view component_type_name = "PointLightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;
    float range = 30.0f;

    bool spot = false;
    float spot_inner_angle = 30.0f;
    float spot_outer_angle = 60.0f;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
    float shadow_range = 100.0f;
    float shadow_depth_bias_factor = 0.0f;
    float shadow_normal_bias_factor = 0.5f;
};
BI_SREFL(
    type(PointLightComponent),
    field(color, Color{}),
    field(strength, RangeF{}),
    field(range, RangeF{}),
    field(spot),
    field(spot_inner_angle, RangeF{0.0f, 90.0f}),
    field(spot_outer_angle, RangeF{0.0f, 90.0f}),
    field(cast_shadow),
    field(shadow_strength, RangeF{0.0f, 1.0f}),
    field(shadow_range, RangeF{}),
    field(shadow_depth_bias_factor, RangeF{0.0f, 1.0f}),
    field(shadow_normal_bias_factor, RangeF{0.0f, 1.0f})
)

struct RectLightComponent final {
    static constexpr std::string_view component_type_name = "RectLightComponent";

    float3 color = float3{1.0f};
    float strength = 1.0f;
    rt::TAssetPtr<TextureAsset> texture;

    float range = 30.0f;

    float width = 1.0f;
    float height = 1.0f;
    bool two_sided = false;

    bool cast_shadow = false;
    float shadow_strength = 1.0f;
    float shadow_range = 100.0f;
    float shadow_depth_bias_factor = 0.0f;
    float shadow_normal_bias_factor = 0.5f;
};
BI_SREFL(
    type(RectLightComponent),
    field(color, Color{}),
    field(strength, RangeF{}),
    field(texture),
    field(range, RangeF{}),
    field(width, RangeF{}),
    field(height, RangeF{}),
    field(two_sided),
    field(cast_shadow),
    field(shadow_strength, RangeF{0.0f, 1.0f}),
    field(shadow_range, RangeF{}),
    field(shadow_depth_bias_factor, RangeF{0.0f, 1.0f}),
    field(shadow_normal_bias_factor, RangeF{0.0f, 1.0f})
)

}

#pragma once

#include <bisemutum/math/math.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/graphics/handles.hpp>
#include <bisemutum/scene_basic/texture.hpp>

namespace bi {

// This struct must be the same as that in "light_struct.hlsl"
struct DirLightData final {
    float3 emission = float3{0.0f};
    int sm_index = -1;
    // direction that points to the light
    float3 direction = float3{0.0f, 0.0f, 1.0f};
    float shadow_strength = 1.0f;
    float4 cascade_shadow_radius_sqr = float4{0.0f};
    float shadow_depth_bias = 0.001f;
    float shadow_normal_bias = 0.0f;
    float2 _pad1;
};
// This struct must be the same as that in "light_struct.hlsl"
struct PointLightData final {
    float3 emission = float3{0.0f};
    float range_sqr_inv = 1.0f;
    float3 position = float3{0.0f};
    float cos_inner = 1.0f;
    // direction that points to the light
    float3 direction = float3{0.0f, 0.0f, 1.0f};
    float cos_outer = 0.0f;
    int sm_index = -1;
    float shadow_strength = 1.0f;
    float shadow_depth_bias = 0.001f;
    float shadow_normal_bias = 0.0f;
};
// This struct must be the same as that in "light_struct.hlsl"
struct RectLightData {
    float3 emission = float3{0.0f};
    int texture_index = -1;
    float3 center_position = float3{0.0f};
    uint32_t two_sided = 0;
    float3 position0 = float3{0.0f};
    float inv_width_sqr;
    float3 position1 = float3{0.0f};
    float inv_height_sqr;
    float3 position2 = float3{0.0f};
    float inv_texel_size = 0.0f;
    float3 position3 = float3{0.0f};
    float _pad1;
    float3 normal = float3{0.0f};
    float _pad2;
};

struct ShadowLight final {
    gfx::Camera camera;
    float shadow_size;
};

struct ShadowMapTextures final {
    gfx::TextureHandle dir_lights_shadow_map;
    gfx::TextureHandle point_lights_shadow_map;
};

inline constexpr size_t max_num_rect_light_textures = 16;

BI_SHADER_PARAMETERS_BEGIN(LightsContextShaderData)
    BI_SHADER_PARAMETER(uint, num_dir_lights)
    BI_SHADER_PARAMETER(uint, num_point_lights)
    BI_SHADER_PARAMETER(uint, num_rect_lights)

    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<DirLightData>, dir_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<PointLightData>, point_lights)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<RectLightData>, rect_lights)

    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, dir_lights_shadow_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, dir_lights_shadow_map)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, point_lights_shadow_transform)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, point_lights_shadow_map)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, shadow_map_sampler)

    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, rect_light_textures, [max_num_rect_light_textures])
    BI_SHADER_PARAMETER_SAMPLER_ARRAY(SamplerState, rect_light_samplers, [max_num_rect_light_textures])

    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture3D, ltc_matrix_lut0)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture3D, ltc_matrix_lut1)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture3D, ltc_matrix_lut2)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture3D, ltc_norm_lut)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, ltc_sampler)
BI_SHADER_PARAMETERS_END()

struct LightsContext final {
    LightsContext();

    auto collect_all_lights() -> void;

    auto update_shader_params() -> void;

    auto prepare_dir_lights_per_camera(gfx::Camera const& camera) -> void;

    std::vector<DirLightData> dir_lights;
    gfx::Buffer dir_lights_buffer;
    std::vector<PointLightData> point_lights;
    gfx::Buffer point_lights_buffer;
    std::vector<RectLightData> rect_lights;
    gfx::Buffer rect_lights_buffer;

    std::vector<ShadowLight> dir_lights_with_shadow;
    gfx::Texture dir_lights_shadow_map;
    std::vector<float4x4> dir_lights_shadow_transform;
    gfx::Buffer dir_lights_shadow_transform_buffer;

    std::vector<ShadowLight> point_lights_with_shadow;
    gfx::Texture point_lights_shadow_map;
    std::vector<float4x4> point_lights_shadow_transform;
    gfx::Buffer point_lights_shadow_transform_buffer;

    Ptr<gfx::Sampler> shadow_map_sampler;
    uint32_t dir_light_shadow_map_resolution;
    uint32_t point_light_shadow_map_resolution;

    std::vector<Ref<TextureAsset>> rect_light_textures;

    Ptr<gfx::Texture> ltc_matrix_lut0;
    Ptr<gfx::Texture> ltc_matrix_lut1;
    Ptr<gfx::Texture> ltc_matrix_lut2;
    Ptr<gfx::Texture> ltc_norm_lut;
    Ptr<gfx::Sampler> ltc_lut_sampler;

    LightsContextShaderData shader_data;
};

}

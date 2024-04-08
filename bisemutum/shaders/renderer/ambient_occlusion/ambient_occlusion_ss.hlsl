#include <bisemutum/shaders/core/vertex_attributes.hlsl>
#include <bisemutum/shaders/core/utils/random.hlsl>
#include <bisemutum/shaders/core/utils/sampling.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float4 ambient_occlusion_ss_fs(VertexAttributesOutput fin) : SV_Target {
    float depth = depth_tex.Sample(input_sampler, fin.texcoord).x;
    if (depth == 1.0) {
        return float4(1.0, 0.0, 0.0, 0.0);
    }

    float4 normal_roughness = normal_roughness_tex.Sample(input_sampler, fin.texcoord);
    float3 normal;
    float3 tangent;
    unpack_normal_and_tangent(normal_roughness.xyz, normal, tangent);
    Frame frame = create_frame(normal, tangent);

    float3 position_view = position_view_from_depth(fin.texcoord, depth, matrix_inv_proj);
    float3 position_world = mul(matrix_inv_view, float4(position_view, 1.0)).xyz;

    uint rng_seed = rng_tea(uint(fin.sv_position.y) * viewport_size.x + uint(fin.sv_position.x), frame_index);
    float ao_value = 0.0;
    [unroll]
    for (uint i = 0; i < 4; i++) {
        float2 rand = float2(rng_next(rng_seed), rng_next(rng_seed));
        float3 dir_local = cos_hemisphere_sample(rand);
        float3 dir_world = frame_to_world(frame, dir_local);

        float3 pos = position_world + dir_world * ao_range;
        float4 pos_clip = mul(matrix_proj_view, float4(pos, 1.0));
        pos_clip.xyz /= pos_clip.w;
        float2 uv = float2(pos_clip.x * 0.5 + 0.5, 0.5 - pos_clip.y * 0.5);
        float tap_depth = depth_tex.Sample(input_sampler, uv).x;

        ao_value += pos_clip.z < tap_depth ? 0.0 : 1.0;
    }
    ao_value = 1.0 - ao_value * ao_strength / 4;

    return float4(ao_value, 1.0, 0.0, 0.0);
}

#include <bisemutum/shaders/core/utils/math.hlsl>
#include <bisemutum/shaders/core/utils/pack.hlsl>
#include <bisemutum/shaders/core/utils/depth.hlsl>
#include <bisemutum/shaders/core/utils/projection.hlsl>
#include <bisemutum/shaders/core/utils/frame.hlsl>
#include <bisemutum/shaders/core/material.hlsl>

#include "../gbuffer.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/compute.hlsl>

#define NUM_GROUP_THREADS 16
#define SHARED_DATA_WIDTH (NUM_GROUP_THREADS + 2)

groupshared float4 s_value[SHARED_DATA_WIDTH * SHARED_DATA_WIDTH];
groupshared float s_depth[SHARED_DATA_WIDTH * SHARED_DATA_WIDTH];
groupshared float3 s_normal[SHARED_DATA_WIDTH * SHARED_DATA_WIDTH];

uint shared_data_index_of(int2 offset) {
    return (offset.y + 1) * SHARED_DATA_WIDTH + offset.x + 1;
}

void fetch_shared_data(uint index, int2 group_start) {
    int2 offset = int2(
        (int) (index % SHARED_DATA_WIDTH) - 1,
        (int) (index / SHARED_DATA_WIDTH) - 1
    );

    s_value[index] = hit_uvz_pdf_tex.Load(int3(group_start + offset, 0));
    float2 uv = (group_start + offset + 0.5) / tex_size;
    s_depth[index] = get_linear_01_depth(depth_tex.SampleLevel(gbuffer_sampler, uv, 0).x, matrix_inv_proj);
    s_normal[index] = oct_decode(gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].SampleLevel(gbuffer_sampler, uv, 0).xy);
}

[numthreads(NUM_GROUP_THREADS, NUM_GROUP_THREADS, 1)]
void ssr_spatial_filter_cs(
    uint3 global_thread_id : SV_DispatchThreadID,
    uint3 local_thread_id : SV_GroupThreadID,
    uint3 group_id : SV_GroupID
) {
    uint linear_local_index = local_thread_id.y * NUM_GROUP_THREADS + local_thread_id.x;
    if (linear_local_index * 2 < SHARED_DATA_WIDTH * SHARED_DATA_WIDTH) {
        fetch_shared_data(linear_local_index * 2, group_id.xy * NUM_GROUP_THREADS);
        fetch_shared_data(linear_local_index * 2 + 1, group_id.xy * NUM_GROUP_THREADS);
    }
    GroupMemoryBarrierWithGroupSync();

    if (any(global_thread_id.xy >= tex_size)) { return; }

    const float2 texcoord = (global_thread_id.xy + 0.5) / tex_size;

    uint center_shared_data_index = shared_data_index_of((int2) local_thread_id.xy);
    float4 center_value = s_value[center_shared_data_index];
    float center_depth = s_depth[center_shared_data_index];
    float3 center_normal = s_normal[center_shared_data_index];

    if (center_value.w == 0.0) {
        reflection_tex[global_thread_id.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    GBuffer gbuffer;
    gbuffer.base_color = gbuffer_textures[GBUFFER_BASE_COLOR].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.normal_roughness = gbuffer_textures[GBUFFER_NORMAL_ROUGHNESS].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.fresnel = gbuffer_textures[GBUFFER_FRESNEL].SampleLevel(gbuffer_sampler, texcoord, 0);
    gbuffer.material_0 = gbuffer_textures[GBUFFER_MATERIAL_0].SampleLevel(gbuffer_sampler, texcoord, 0);

    float3 N, T;
    SurfaceData surface;
    uint surface_model;
    unpack_gbuffer_to_surface(gbuffer, N, T, surface, surface_model);
    float3 B = cross(N, T);

    float3 pos_world = position_world_from_linear_01_depth(texcoord, center_depth, matrix_inv_proj, matrix_inv_view);
    float3 V = normalize(camera_position_world() - pos_world);

    const float kernel_1d[3] = {0.27901, 0.44198, 0.27901};

    float3 result = 0.0;
    float weight_sum = 0.0;
    float3 bsdf_diffuse;
    float3 bsdf_specular;
    [unroll]
    for (int y = -1; y <= 1; y++) {
        [unroll]
        for (int x = -1; x <= 1; x++) {
            uint shared_data_index = shared_data_index_of((int2) local_thread_id.xy + int2(x, y));
            float4 hit_uvz_pdf = s_value[shared_data_index];
            if (hit_uvz_pdf.w == 0.0) { continue; }

            float3 hit_pos_world = position_world_from_linear_01_depth(hit_uvz_pdf.xy, hit_uvz_pdf.z, matrix_inv_proj, matrix_inv_view);
            float3 out_dir = hit_pos_world - pos_world;
            float hit_dist = length(out_dir);
            out_dir /= hit_dist;
            surface_eval(N, T, B, V, out_dir, surface, surface_model, bsdf_diffuse, bsdf_specular);

            float3 color = bsdf_specular * color_tex.SampleLevel(gbuffer_sampler, hit_uvz_pdf.xy, 0).xyz * exp(-hit_dist);

            float depth = s_depth[shared_data_index];
            float3 normal = s_normal[shared_data_index];
            float weight = kernel_1d[x + 1] * kernel_1d[y + 1]
                * max(dot(center_normal, normal), 0.0)
                * exp(-abs(depth - center_depth) / (max(depth, center_depth) + 0.001));
            result += color * weight;
            weight_sum += weight;
        }
    }
    result = weight_sum == 0.0 ? 0.0 : result / weight_sum;

    float fade = 1.0 - max(surface.roughness - fade_roughness, 0.0) / max(max_roughness - fade_roughness, 0.0001);
    result *= fade;

    reflection_tex[global_thread_id.xy] = float4(result, 1.0);
}

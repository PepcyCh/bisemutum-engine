#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include "gbuffer.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/material.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

struct GBufferOutput {
    float4 color : SV_Target0;
    float4 base_color : SV_Target1;
    float4 normal_roughness : SV_Target2;
    float4 fresnel : SV_Target3;
    float4 material_0 : SV_Target4;
    float4 velocity : SV_Target5;
};

GBufferOutput gbuffer_pass_fs(VertexAttributesOutput fin) {
    float3 N = normalize(fin.normal_world);
    float3 T = normalize(fin.tangent_world);
    float3 B = normalize(fin.bitangent_world);
    float3 V = normalize(camera_position_world() - fin.position_world);

    SurfaceData surface = material_function(fin);
#ifdef MATERIAL_BLEND_MODE_ALPHA_TEST
    if (surface.opacity < ALPHA_TEST_OPACITY_THRESHOLD) {
        discard;
    }
#endif

    float3 normal_tspace = surface.normal_map_value * 2.0 - 1.0;
    N = normalize(normal_tspace.x * T + normal_tspace.y * B + normal_tspace.z * N);

    if (surface.two_sided && dot(V, N) < 0.0) {
        N = -N;
    }

    GBufferOutput fout;
    fout.color = dot(V, N) < 0.0 ? 0.0 : float4(surface.emission, 1.0);

    GBuffer gbuffer = pack_surface_to_gbuffer(V, N, T, surface, MATERIAL_SURFACE_MODEL);
    fout.base_color = gbuffer.base_color;
    fout.normal_roughness = gbuffer.normal_roughness;
    fout.fresnel = gbuffer.fresnel;
    fout.material_0 = gbuffer.material_0;

    float4 history_pos_clip = mul(history_matrix_proj_view, float4(fin.history_position_world, 1.0));
    history_pos_clip.xyz /= history_pos_clip.w;
    float2 history_uv = float2(history_pos_clip.x * 0.5 + 0.5, 0.5 - history_pos_clip.y * 0.5);
    fout.velocity = float4(fin.sv_position.xy / viewport_size - history_uv, 0.0, 1.0);

    return fout;
}

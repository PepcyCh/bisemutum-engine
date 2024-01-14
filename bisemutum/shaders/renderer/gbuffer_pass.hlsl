#include "../core/vertex_attributes.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/material.hlsl"
#include "../core/shader_params/fragment.hlsl"

#include "gbuffer.hlsl"

struct GBufferOutput {
    float4 base_color : SV_Target0;
    float4 normal_roughness : SV_Target1;
    float4 fresnel : SV_Target2;
    float4 material_0 : SV_Target3;
};

GBufferOutput gbuffer_pass_fs(VertexAttributesOutput fin) {
    float3 N = normalize(fin.normal_world);
    float3 T = normalize(fin.tangent_world);
    float3 B = normalize(fin.bitangent_world);
    float3 V = normalize(camera_position_world() - fin.position_world);

    SurfaceData surface = material_function(fin);
    float3 normal_tspace = surface.normal_map_value * 2.0 - 1.0;
    N = normalize(normal_tspace.x * T + normal_tspace.y * B + normal_tspace.z * N);

    GBuffer gbuffer = pack_surface_to_gbuffer(V, N, T, surface, MATERIAL_SURFACE_MODEL);
    GBufferOutput fout;
    fout.base_color = gbuffer.base_color;
    fout.normal_roughness = gbuffer.normal_roughness;
    fout.fresnel = gbuffer.fresnel;
    fout.material_0 = gbuffer.material_0;

    return fout;
}

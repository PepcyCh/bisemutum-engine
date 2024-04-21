#include <bisemutum/shaders/core/raytracing/hit.hlsl>
#include <bisemutum/shaders/core/raytracing/attributes.hlsl>

#include "rt_gbuffer.hlsl"

[shader("closesthit")]
void rt_gbuffer_rchit(inout GBufferPayload payload, HitAttributes attribs) {
    VertexAttributesOutput vert = fetch_vertex_attributes(attribs.bary);
    SurfaceData surface = material_function(vert);
    float3 normal_tspace = surface.normal_map_value * 2.0 - 1.0;
    float3 N = normalize(normal_tspace.x * vert.tangent_world + normal_tspace.y * vert.bitangent_world + normal_tspace.z * vert.normal_world);
    if (surface.two_sided && dot(WorldRayDirection(), N) > 0.0) {
        N = -N;
    }
    payload.gbuffer = pack_surface_to_gbuffer(-WorldRayDirection(), N, vert.tangent_world, surface, MATERIAL_SURFACE_MODEL);
    payload.hit_t = RayTCurrent();
    payload.history_position = vert.history_position_world;
}

[shader("anyhit")]
void rt_gbuffer_rahit(inout GBufferPayload payload, HitAttributes attribs) {
#ifndef MATERIAL_BLEND_MODE_OPAQUE
    VertexAttributesOutput vert = fetch_vertex_attributes(attribs.bary);
    SurfaceData surface = material_function(vert);
#ifdef MATERIAL_BLEND_MODE_ALPHA_TEST
    if (surface.opacity < ALPHA_TEST_OPACITY_THRESHOLD) {
        IgnoreHit();
    }
#else
    if (payload.opacity_random < 1.0 - surface.opacity) {
        IgnoreHit();
    }
#endif
#endif
}

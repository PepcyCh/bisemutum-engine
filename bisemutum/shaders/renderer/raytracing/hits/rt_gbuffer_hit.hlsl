#include "rt_gbuffer.hlsl"
#include "../../../core/raytracing/hit.hlsl"
#include "../../../core/raytracing/attributes.hlsl"

[shader("closesthit")]
void rt_gbuffer_rchit(inout GBufferPayload payload, HitAttributes attribs) {
    VertexAttributesOutput vert = fetch_vertex_attributes(attribs.bary);
    SurfaceData surface = material_function(vert);
    payload.gbuffer = pack_surface_to_gbuffer(-WorldRayDirection(), vert.normal_world, vert.tangent_world, surface, MATERIAL_SURFACE_MODEL);
    payload.hit_t = RayTCurrent();
    payload.history_position = vert.history_position_world;
}

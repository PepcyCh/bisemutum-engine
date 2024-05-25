#include <bisemutum/shaders/core/utils/random.hlsl>
#include "hits/rt_gbuffer.hlsl"

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/raytracing.hlsl>

[shader("raygeneration")]
void rt_gbuffer_rgen() {
    uint2 pixel_coord = DispatchRaysIndex().xy;

    float3 ray_origin = ray_origins.Load(int3(pixel_coord, 0)).xyz;
    float3 ray_direction = ray_directions.Load(int3(pixel_coord, 0)).xyz;
    if (all(ray_direction == 0.0)) {
        return;
    }

    RayDesc ray;
    ray.TMin = 0.001;
    ray.TMax = ray_length;
    ray.Origin = ray_origin;
    ray.Direction = ray_direction;

    GBufferPayload gbuffer;
    gbuffer.opacity_random = gen_opacity_random_for(ray_origin, ray_direction, frame_index);
    TraceRay(scene_accel, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, gbuffer);

    if (gbuffer.hit_t > 0.0) {
        gbuffer_base_color[pixel_coord] = gbuffer.gbuffer.base_color;
        gbuffer_normal_roughness[pixel_coord] = gbuffer.gbuffer.normal_roughness;
        gbuffer_fresnel[pixel_coord] = gbuffer.gbuffer.fresnel;
        gbuffer_material_0[pixel_coord] = gbuffer.gbuffer.material_0;
        hit_positions[pixel_coord] = float4(ray_origin + ray_direction * gbuffer.hit_t, gbuffer.hit_t);
    } else {
        hit_positions[pixel_coord] = float4(ray_direction, -1.0);
    }
}

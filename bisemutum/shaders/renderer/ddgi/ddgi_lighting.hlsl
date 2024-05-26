#pragma once

#include <bisemutum/shaders/core/utils/pack.hlsl>

#include "ddgi_struct.hlsl"

float4 calc_ddgi_volume_lighting(
    float3 pos, float3 normal, float3 view,
    DdgiVolumeData volume,
    uint volume_index,
    Texture2DArray irradiance_texture,
    Texture2DArray visibility_texture,
    SamplerState sampler
) {
    pos += normal * 0.2 + view * 0.8;

    float3 vec = pos - volume.base_position;
    float x = dot(vec, volume.frame_x);
    float y = dot(vec, volume.frame_y);
    float z = dot(vec, volume.frame_z);
    if (
        x < 0.0 || x > volume.extent_x
        || y < 0.0 || y > volume.extent_y
        || z < 0.0 || z > volume.extent_z
    ) {
        return 0.0;
    }

    float2 oct_norm = oct_encode_01(normal);
    float2 irradiance_probe_uv = (oct_norm * DDGI_PROBE_IRRADIANCE_SIZE + 1) / (DDGI_PROBE_IRRADIANCE_SIZE + 2);
    float2 visibility_probe_uv = (oct_norm * DDGI_PROBE_VISIBILITY_SIZE + 1) / (DDGI_PROBE_VISIBILITY_SIZE + 2);

    float3 index_f = float3(
        x * DDGI_PROBES_SIZE_M_1 / volume.extent_x,
        y * DDGI_PROBES_SIZE_M_1 / volume.extent_y,
        z * DDGI_PROBES_SIZE_M_1 / volume.extent_z
    );
    uint3 index = min(uint3(index_f), DDGI_PROBES_SIZE_M_1 - 1);
    index_f -= index;

    float3 sum = 0.0;
    float sum_weight = 0.0;
    [unroll]
    for (uint i = 0; i < 8; i++) {
        uint dx = i & 1;
        uint dy = (i >> 1) & 1;
        uint dz = i >> 2;
        float3 w_probe3 = lerp(1.0f - index_f, index_f, float3(dx, dy, dz));
        float w_probe = w_probe3.x * w_probe3.y * w_probe3.z;

        uint3 probe_index = index + uint3(dx, dy, dz);
        float3 probe_center = volume.base_position
            + probe_index.x * volume.extent_x / DDGI_PROBES_SIZE_M_1 * volume.frame_x
            + probe_index.y * volume.extent_y / DDGI_PROBES_SIZE_M_1 * volume.frame_y
            + probe_index.z * volume.extent_z / DDGI_PROBES_SIZE_M_1 * volume.frame_z;
        float3 dir = normalize(probe_center - pos);
        float w_dir = pow2((dot(dir, normal) + 1.0) * 0.5) + 0.2;

        float2 probe_start = float2(probe_index.y * DDGI_PROBES_SIZE + probe_index.x, probe_index.z);

        float3 visibility_uv = float3(
            (probe_start + visibility_probe_uv) / float2(DDGI_PROBES_SIZE * DDGI_PROBES_SIZE, DDGI_PROBES_SIZE),
            volume_index
        );
        float2 visibility = visibility_texture.SampleLevel(sampler, visibility_uv, 0).xy;
        float sigma2 = visibility.y - pow2(visibility.x);
        float dist = length(probe_center - pos);
        float w_vis = sigma2 / (sigma2 + pow2(max(dist - visibility.x, 0.0)));

        float3 irradiance_uv = float3(
            (probe_start + irradiance_probe_uv) / float2(DDGI_PROBES_SIZE * DDGI_PROBES_SIZE, DDGI_PROBES_SIZE),
            volume_index
        );
        float3 irradiance = irradiance_texture.SampleLevel(sampler, irradiance_uv, 0).xyz;
        float w = w_probe * w_dir * pow3(w_vis);
        sum += irradiance * w;
        sum_weight += w;
    }

    float4 result = float4(sum_weight == 0.0 ? 0.0 : sum / sum_weight, 1.0);
    if (any(isnan(result)) || any(isinf(result))) { result = 0.0; }
    return result;
}

#pragma once

#define DDGI_PROBES_SIZE 8
#define DDGI_PROBES_COUNT (DDGI_PROBES_SIZE * DDGI_PROBES_SIZE * DDGI_PROBES_SIZE)

#define DDGI_NUM_RAYS_PER_PROBE 64

#define DDGI_PROBE_IRRADIANCE_SIZE 6
#define DDGI_PROBE_VISIBILITY_SIZE 14

#define DDGI_TEMPORAL_ACCUMULATE_ALPHA 0.97
#define DDGI_TEMPORAL_ACCUMULATE_GAMMA 5.0

#define DDGI_SAMPLE_RANDOM_SIZE 8192

struct DdgiVolumeData {
    float3 base_position;
    float _pad;
    float3 frame_x;
    float extent_x;
    float3 frame_y;
    float extent_y;
    float3 frame_z;
    float extent_z;
};

bool calc_ddgi_volume_probe_index(float3 pos, DdgiVolumeData volume, out float3 index) {
    float3 vec = pos - volume.base_position;
    float x = dot(vec, volume.frame_x);
    float y = dot(vec, volume.frame_y);
    float z = dot(vec, volume.frame_z);
    if (
        x >= 0.0 && x <= volume.extent_x
        && y >= 0.0 && y <= volume.extent_y
        && z >= 0.0 && z <= volume.extent_z
    ) {
        index = float3(x, y, z);
        return true;
    } else {
        return false;
    }
}

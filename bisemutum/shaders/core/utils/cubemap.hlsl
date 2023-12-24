#pragma once

float3 cubemap_direction_from_layered_uv(float2 uv, uint layer) {
    uv = uv * 2.0 - 1.0;
    float3 dir;
    if (layer == 0) { // +X
        dir = float3(1.0, -uv.y, -uv.x);
    } else if (layer == 1) { // -X
        dir = float3(-1.0, -uv.y, uv.x);
    } else if (layer == 2) { // +Y
        dir = float3(uv.x, 1.0, uv.y);
    } else if (layer == 3) { // -Y
        dir = float3(uv.x, -1.0, -uv.y);
    } else if (layer == 4) { // +Z
        dir = float3(uv.x, -uv.y, 1.0);
    } else { // -Z
        dir = float3(-uv.x, -uv.y, -1.0);
    }
    dir = normalize(dir);
    return dir;
}

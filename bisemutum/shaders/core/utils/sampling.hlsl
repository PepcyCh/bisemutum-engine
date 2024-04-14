#pragma once

#include "../utils/math.hlsl"

float3 uniform_hemisphere_sample(float2 rand) {
    float phi = TWO_PI * rand.x;
    float r = sqrt(max(1.0 - rand.y * rand.y, 0.0));
    return float3(cos(phi) * r, sin(phi) * r, rand.y);
}
float uniform_hemisphere_pdf() {
    return INV_TWO_PI;
}

float3 cos_hemisphere_sample(float2 rand) {
    float phi = rand.x * TWO_PI;
    float r = sqrt(rand.y);
    return float3(cos(phi) * r, sin(phi) * r, sqrt(max(1.0 - rand.y, 0.0)));
}
float cos_hemisphere_pdf(float3 p) {
    return abs(p.z * INV_PI);
}

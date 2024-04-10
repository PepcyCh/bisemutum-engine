#pragma once

#define DEVICE_Z_NEAREST 1.0
#define DEVICE_Z_FARTHEST 0.0

bool is_depth_nearer(float za, float zb) {
    return za > zb;
}
bool is_depth_farther(float za, float zb) {
    return za < zb;
}

float get_nearer_depth(float za, float zb) {
    return max(za, zb);
}
float get_farther_depth(float za, float zb) {
    return min(za, zb);
}

bool is_depth_background(float z) {
    return z == DEVICE_Z_FARTHEST;
}

float depth_pull(float z, float delta) {
    return z + delta;
}
float depth_push(float z, float delta) {
    return z - delta;
}

float get_linear_01_depth(float depth, float4x4 inv_proj) {
    const float a = inv_proj[2].w;
    const float b = inv_proj[3].w;
    return (1.0 - depth) * b / (a * depth + b);
}

float get_linear_depth(float depth, float4x4 inv_proj) {
    const float a = inv_proj[2].w;
    const float b = inv_proj[3].w;
    return 1.0 / (a * depth + b);
}

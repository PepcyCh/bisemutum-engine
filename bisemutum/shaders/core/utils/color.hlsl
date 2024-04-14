#pragma once

float luminance(float3 s) {
    return s[0] * 0.212671f + s[1] * 0.715160f + s[2] * 0.072169f;
}

float3 xyz_to_rgb(float3 xyz) {
    return float3(
        3.240479 * xyz.x - 1.537150 * xyz.y - 0.498535 * xyz.z,
        -0.969256 * xyz.x + 1.875991 * xyz.y + 0.041556 * xyz.z,
        0.055648 * xyz.x - 0.204043 * xyz.y + 1.057311 * xyz.z
    );
}

float3 rgb_to_xyz(float3 rgb) {
    return float3(
        0.412453 * rgb.r + 0.357580 * rgb.y + 0.180423 * rgb.z,
        0.212671 * rgb.r + 0.715160 * rgb.y + 0.072169 * rgb.z,
        0.019334 * rgb.r + 0.119193 * rgb.y + 0.950227 * rgb.z
    );
}

float3 rgb_to_ycocg(float3 rgb) {
    return float3(
        0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
        0.5 * rgb.r - 0.5 * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

float3 ycocg_to_rgb(float3 ycocg) {
    return float3(
        ycocg.r + ycocg.g - ycocg.b,
        ycocg.r + ycocg.b,
        ycocg.r - ycocg.g - ycocg.b
    );
}

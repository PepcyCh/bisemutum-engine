#pragma once

static const float PI = 3.14159265359;
static const float TWO_PI = 2.0 * PI;
static const float FOUR_PI = 4.0 * PI;
static const float INV_PI = 1.0 / PI;
static const float INV_TWO_PI = 1.0 / TWO_PI;
static const float INV_FOUR_PI = 1.0 / FOUR_PI;

float pow2(float x) {
    return x * x;
}

float pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x;
}

float luminance(float3 s) {
    return s.x * 0.212671f + s.y * 0.715160f + s.z * 0.072169f;
}

#pragma once

static const float PI = 3.14159265359f;
static const float INV_PI = 1.0 / PI;

float pow2(float x) {
    return x * x;
}

float pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x;
}

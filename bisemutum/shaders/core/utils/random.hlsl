#pragma once

uint rng_tea(uint val0, uint val1) {
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for (uint n = 0; n < 16; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

uint rng_lcg(inout uint prev) {
    const uint A = 1664525u;
    const uint C = 1013904223u;
    prev = (A * prev + C);
    return prev & 0x00ffffff;
}

float rng_next(inout uint prev) {
    return (float(rng_lcg(prev)) / float(0x01000000));
}

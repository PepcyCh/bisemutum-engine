#pragma once

// Extract even bits (start from 0 from low bit) of x.
// For example, 0x12345678 -> 0x000046ec
uint extract_even_bits(uint x) {
    x &= 0x55555555;
    x = (x | (x >> 1)) & 0x33333333;
    x = (x | (x >> 2)) & 0x0f0f0f0f;
    x = (x | (x >> 4)) & 0x00ff00ff;
    x = (x | (x >> 8)) & 0x0000ffff;
    return x;
}

// This is a bit of an inverse operation of 'extract_even_bits'
// For example, 0x00001234 -> 0x01040510
uint64_t interleave_bits_with_zeros(const uint in_x)  {
    uint64_t x = in_x;
    x = (x ^ (x << 16)) & 0x0000ffff0000fffful;
    x = (x ^ (x << 8))  & 0x00ff00ff00ff00fful;
    x = (x ^ (x << 4))  & 0x0f0f0f0f0f0f0f0ful;
    x = (x ^ (x << 2))  & 0x3333333333333333ul;
    x = (x ^ (x << 1))  & 0x5555555555555555ul;
    return x;
}
uint64_t interleave_bits(const uint a, const uint b)  {
    return interleave_bits_with_zeros(a) | (interleave_bits_with_zeros(b) << 1);
}

// xor of all bits of x
uint xor_bits(uint x) {
    x ^= x >> 1;
    x ^= x >> 2;
    x ^= x >> 4;
    x ^= x >> 8;
    return x;
}

// A random number extraction (1->2) algorithm from section 1.4.1 of
//   'The Implementation of A Hair Scattering Model' by Matt Pharr
// https://pbrt.org/hair.pdf
float2 demux(float f) {
    uint64_t v = uint64_t(f * float(uint64_t(1) << 32));
    return float2(extract_even_bits(uint(v)) / float(1 << 16), extract_even_bits(uint(v >> 1)) / float(1 << 16));
}

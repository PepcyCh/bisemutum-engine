#pragma once

#include <cstdint>

namespace bi::rhi {

enum class Backend : uint8_t {
    vulkan,
    d3d12,
};

struct Extent3D final {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth_or_layers = 1;

    auto operator==(Extent3D const& rhs) const -> bool = default;
};
struct Offset3D final {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;

    auto operator==(Offset3D const& rhs) const -> bool = default;
};

enum class IndexType : uint8_t {
    uint16,
    uint32,
};

enum class CompareOp : uint8_t {
    never,
    less,
    equal,
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always,
};

// Values are the same as those in Vulkan
enum class ResourceFormat : uint8_t {
    undefined = 0,
    rg4_unorm = 1,
    rgba4_unorm = 2,
    bgra4_unorm = 3,
    rgb565_unorm = 4,
    bgr565_unorm = 5,
    rgb5a1_unorm = 6,
    bgr5a1_unorm = 7,
    a1rgb5_unorm = 8,
    r8_unorm = 9,
    r8_snorm = 10,
    r8_uint = 13,
    r8_sint = 14,
    r8_srgb = 15,
    rg8_unorm = 16,
    rg8_snorm = 17,
    rg8_uint = 20,
    rg8_sint = 21,
    rg8_srgb = 22,
    rgba8_unorm = 37,
    rgba8_snorm = 38,
    rgba8_uint = 41,
    rgba8_sint = 42,
    rgba8_srgb = 43,
    bgra8_unorm = 44,
    bgra8_snorm = 45,
    bgra8_uint = 48,
    bgra8_sint = 49,
    bgra8_srgb = 50,
    abgr8_unorm = 51,
    abgr8_snorm = 52,
    abgr8_uint = 55,
    abgr8_sint = 56,
    abgr8_srgb = 57,
    a2rgb10_unorm = 58,
    a2rgb10_snorm = 59,
    a2rgb10_uint = 62,
    a2rgb10_sint = 63,
    a2bgr10_unorm = 64,
    a2bgr10_snorm = 65,
    a2bgr10_uint = 68,
    a2bgr10_sint = 69,
    r16_unorm = 70,
    r16_snorm = 71,
    r16_uint = 74,
    r16_sint = 75,
    r16_sfloat = 76,
    rg16_unorm = 77,
    rg16_snorm = 78,
    rg16_uint = 81,
    rg16_sint = 82,
    rg16_sfloat = 83,
    rgba16_unorm = 91,
    rgba16_snorm = 92,
    rgba16_uint = 95,
    rgba16_sint = 96,
    rgba16_sfloat = 97,
    r32_uint = 98,
    r32_sint = 99,
    r32_sfloat = 100,
    rg32_uint = 101,
    rg32_sint = 102,
    rg32_sfloat = 103,
    rgb32_uint = 104,
    rgb32_sint = 105,
    rgb32_sfloat = 106,
    rgba32_uint = 107,
    rgba32_sint = 108,
    rgba32_sfloat = 109,
    b10gr11_ufloat = 122,
    e5rgb9_ufloat = 123,
    d16_unorm = 124,
    x8d24_unorm = 125,
    d32_sfloat = 126,
    d16_unorm_s8_uint = 128,
    d24_unorm_s8_uint = 129,
    d32_sfloat_s8_uint = 130,
    bc1_rgb_unorm = 131,
    bc1_rgb_srgb = 132,
    bc1_rgba_unorm = 133,
    bc1_rgba_srgb = 134,
    bc2_unorm = 135,
    bc2_srgb = 136,
    bc3_unorm = 137,
    bc3_srgb = 138,
    bc4_unorm = 139,
    bc4_snorm = 140,
    bc5_unorm = 141,
    bc5_snorm = 142,
    bc6h_ufloat = 143,
    bc6h_sfloat = 144,
    bc7_unorm = 145,
    bc7_sgrb = 146,
};

inline auto is_color_format(ResourceFormat format) -> bool {
    return format != ResourceFormat::d16_unorm
        && format != ResourceFormat::d16_unorm_s8_uint
        && format != ResourceFormat::d24_unorm_s8_uint
        && format != ResourceFormat::d32_sfloat
        && format != ResourceFormat::d32_sfloat_s8_uint
        && format != ResourceFormat::x8d24_unorm;
}
inline auto is_depth_stencil_format(ResourceFormat format) -> bool {
    return !is_color_format(format);
}
inline auto is_depth_only_format(ResourceFormat format) -> bool {
    return format == ResourceFormat::d16_unorm
        || format == ResourceFormat::d32_sfloat
        || format == ResourceFormat::x8d24_unorm;
}
inline auto is_srgb_format(ResourceFormat format) -> bool {
    return format == ResourceFormat::r8_srgb
        || format == ResourceFormat::rg8_srgb
        || format == ResourceFormat::rgba8_srgb
        || format == ResourceFormat::bgra8_srgb
        || format == ResourceFormat::abgr8_srgb
        || format == ResourceFormat::bc1_rgb_srgb
        || format == ResourceFormat::bc1_rgba_srgb
        || format == ResourceFormat::bc2_srgb
        || format == ResourceFormat::bc3_srgb
        || format == ResourceFormat::bc7_sgrb;
}
inline auto is_compressed_format(ResourceFormat format) -> bool {
    return static_cast<uint8_t>(format) >= static_cast<uint8_t>(ResourceFormat::bc1_rgb_unorm);
}

auto format_texel_size(ResourceFormat format) -> uint32_t;

}

#include <bisemutum/rhi/defines.hpp>

namespace bi::rhi {

auto format_texel_size(ResourceFormat format) -> uint32_t {
    switch (format) {
        case ResourceFormat::undefined:
            return 0;
        case ResourceFormat::rg4_unorm:
            return 1;
        case ResourceFormat::rgba4_unorm:
        case ResourceFormat::bgra4_unorm:
        case ResourceFormat::rgb565_unorm:
        case ResourceFormat::bgr565_unorm:
        case ResourceFormat::rgb5a1_unorm:
        case ResourceFormat::bgr5a1_unorm:
        case ResourceFormat::a1rgb5_unorm:
            return 2;
        case ResourceFormat::r8_unorm:
        case ResourceFormat::r8_snorm:
        case ResourceFormat::r8_uint:
        case ResourceFormat::r8_sint:
        case ResourceFormat::r8_srgb:
            return 1;
        case ResourceFormat::rg8_unorm:
        case ResourceFormat::rg8_snorm:
        case ResourceFormat::rg8_uint:
        case ResourceFormat::rg8_sint:
        case ResourceFormat::rg8_srgb:
            return 2;
        case ResourceFormat::rgba8_unorm:
        case ResourceFormat::rgba8_snorm:
        case ResourceFormat::rgba8_uint:
        case ResourceFormat::rgba8_sint:
        case ResourceFormat::rgba8_srgb:
        case ResourceFormat::bgra8_unorm:
        case ResourceFormat::bgra8_snorm:
        case ResourceFormat::bgra8_uint:
        case ResourceFormat::bgra8_sint:
        case ResourceFormat::bgra8_srgb:
        case ResourceFormat::abgr8_unorm:
        case ResourceFormat::abgr8_snorm:
        case ResourceFormat::abgr8_uint:
        case ResourceFormat::abgr8_sint:
        case ResourceFormat::abgr8_srgb:
        case ResourceFormat::a2rgb10_unorm:
        case ResourceFormat::a2rgb10_snorm:
        case ResourceFormat::a2rgb10_uint:
        case ResourceFormat::a2rgb10_sint:
        case ResourceFormat::a2bgr10_unorm:
        case ResourceFormat::a2bgr10_snorm:
        case ResourceFormat::a2bgr10_uint:
        case ResourceFormat::a2bgr10_sint:
            return 4;
        case ResourceFormat::r16_unorm:
        case ResourceFormat::r16_snorm:
        case ResourceFormat::r16_uint:
        case ResourceFormat::r16_sint:
        case ResourceFormat::r16_sfloat:
            return 2;
        case ResourceFormat::rg16_unorm:
        case ResourceFormat::rg16_snorm:
        case ResourceFormat::rg16_uint:
        case ResourceFormat::rg16_sint:
        case ResourceFormat::rg16_sfloat:
            return 4;
        case ResourceFormat::rgba16_unorm:
        case ResourceFormat::rgba16_snorm:
        case ResourceFormat::rgba16_uint:
        case ResourceFormat::rgba16_sint:
        case ResourceFormat::rgba16_sfloat:
            return 8;
        case ResourceFormat::r32_uint:
        case ResourceFormat::r32_sint:
        case ResourceFormat::r32_sfloat:
            return 4;
        case ResourceFormat::rg32_uint:
        case ResourceFormat::rg32_sint:
        case ResourceFormat::rg32_sfloat:
            return 8;
        case ResourceFormat::rgb32_uint:
        case ResourceFormat::rgb32_sint:
        case ResourceFormat::rgb32_sfloat:
            return 12;
        case ResourceFormat::rgba32_uint:
        case ResourceFormat::rgba32_sint:
        case ResourceFormat::rgba32_sfloat:
            return 16;
        case ResourceFormat::b10gr11_ufloat:
        case ResourceFormat::e5rgb9_ufloat:
            return 4;
        default:
            return 0;
    }
}

}

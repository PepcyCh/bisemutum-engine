#pragma once

#include <cstdint>

#include "mod.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct Extent3D {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth_or_layers = 1;
};
struct Offset3D {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
};

enum class IndexType : uint8_t {
    eUInt16,
    eUInt32,
};

enum class CompareOp : uint8_t {
    eNever,
    eLess,
    eEqual,
    eLessEqual,
    eGreater,
    eNotEqual,
    eGreaterEqual,
    eAlways,
};

// value is the same as that in Vulkan
enum class ResourceFormat : uint8_t {
    eUndefined = 0,
    eRg4UNorm = 1,
    eRgba4UNorm = 2,
    eBgra4UNorm = 3,
    eRgb565UNorm = 4,
    eBgr565UNorm = 5,
    eRgb5A1UNorm = 6,
    eBgr5A1UNorm = 7,
    eA1Rgb5UNorm = 8,
    eR8UNorm = 9,
    eR8SNorm = 10,
    eR8UInt = 13,
    eR8SInt = 14,
    eR8Srgb = 15,
    eRg8UNorm = 16,
    eRg8SNorm = 17,
    eRg8UInt = 20,
    eRg8SInt = 21,
    eRg8Srgb = 22,
    eRgb8UNorm = 23,
    eRgb8SNorm = 24,
    eRgb8UInt = 27,
    eRgb8SInt = 28,
    eRgb8Srgb = 29,
    eBgr8UNorm = 30,
    eBgr8SNorm = 31,
    eBgr8UInt = 34,
    eBgr8SInt = 35,
    eBgr8Srgb = 36,
    eRgba8UNorm = 37,
    eRgba8SNorm = 38,
    eRgba8UInt = 41,
    eRgba8SInt = 42,
    eRgba8Srgb = 43,
    eBgra8UNorm = 44,
    eBgra8SNorm = 45,
    eBgra8UInt = 48,
    eBgra8SInt = 49,
    eBgra8Srgb = 50,
    eAbgr8UNorm = 51,
    eAbgr8SNorm = 52,
    eAbgr8UInt = 55,
    eAbgr8SInt = 56,
    eAbgr8Srgb = 57,
    eA2Rgb10UNorm = 58,
    eA2Rgb10SNorm = 59,
    eA2Rgb10UInt = 62,
    eA2Rgb10SInt = 63,
    eA2Bgr10UNorm = 64,
    eA2Bgr10SNorm = 65,
    eA2Brg10UInt = 68,
    eA2Brg10SInt = 69,
    eR16UNorm = 70,
    eR16SNorm = 71,
    eR16UInt = 74,
    eR16SInt = 75,
    eR16SFloat = 76,
    eRg16UNorm = 77,
    eRg16SNorm = 78,
    eRg16UInt = 81,
    eRg16SInt = 82,
    eRg16SFloat = 83,
    eRgb16UNorm = 84,
    eRgb16SNorm = 85,
    eRgb16UInt = 88,
    eRgb16SInt = 89,
    eRgb16SFloat = 90,
    eRgba16UNorm = 91,
    eRgba16SNorm = 92,
    eRgba16UInt = 95,
    eRgba16SInt = 96,
    eRgba16SFloat = 97,
    eR32UInt = 98,
    eR32SInt = 99,
    eR32SFloat = 100,
    eRg32UInt = 101,
    eRg32SInt = 102,
    eRg32SFloat = 103,
    eRgb32UInt = 104,
    eRgb32SInt = 105,
    eRgb32SFloat = 106,
    eRgba32UInt = 107,
    eRgba32SInt = 108,
    eRgba32SFloat = 109,
    eB10Gr11UFloat = 122,
    eE5Rgb9UFloat = 123,
    eD16UNorm = 124,
    eX8D24UNorm = 125,
    eD32SFloat = 126,
    eD16UNormS8UInt = 128,
    eD24UNormS8UInt = 129,
    eD32SFloatS8UInt = 130,
    eBc1RgbUNorm = 131,
    eBc1RgbSrgb = 132,
    eBc1RgbaUNorm = 133,
    eBc1RgbaSrgb = 134,
    eBc2UNorm = 135,
    eBc2Srgb = 136,
    eBc3UNorm = 137,
    eBc3Srgb = 138,
    eBc4UNorm = 139,
    eBc4SNorm = 140,
    eBc5UNorm = 141,
    eBc5SNorm = 142,
    eBc6HUFloat = 143,
    eBc6HSFLoat = 144,
    eBc7UNorm = 145,
    eBc7Srgb = 146,
};

inline bool IsColorFormat(ResourceFormat format) {
    return format != ResourceFormat::eD16UNorm
        && format != ResourceFormat::eD16UNormS8UInt
        && format != ResourceFormat::eD24UNormS8UInt
        && format != ResourceFormat::eD32SFloat
        && format != ResourceFormat::eD32SFloatS8UInt
        && format != ResourceFormat::eX8D24UNorm;
}
inline bool IsDepthStencilFormat(ResourceFormat format) {
    return !IsColorFormat(format);
}
inline bool IsDepthOnlyFormat(ResourceFormat format) {
    return format == ResourceFormat::eD16UNorm
        || format == ResourceFormat::eD32SFloat
        || format == ResourceFormat::eX8D24UNorm;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

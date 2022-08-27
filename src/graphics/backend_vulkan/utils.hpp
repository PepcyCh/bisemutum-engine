#pragma once

#include <volk.h>

#include "core/utils.hpp"
#include "graphics/defines.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct VkStructHeader {
    VkStructureType sType;
    VkStructHeader *pNext;
};

inline void ConnectVkPNextChain(VkStructHeader *struct1, VkStructHeader *struct2) {
    VkStructHeader *p = struct1;
    while (p->pNext != nullptr) {
        p = p->pNext;
    }
    p->pNext = struct2;
}
template <typename Struct1, typename Struct2>
void ConnectVkPNextChain(Struct1 *struct1, Struct2 *struct2) {
    ConnectVkPNextChain(reinterpret_cast<VkStructHeader *>(struct1), reinterpret_cast<VkStructHeader *>(struct2));
}

inline VkFormat ToVkFormat(ResourceFormat format) {
    return static_cast<VkFormat>(format);
}
inline ResourceFormat FromVkFormat(VkFormat format) {
    return static_cast<ResourceFormat>(format);
}

inline uint32_t FormatTexelSize(ResourceFormat format) {
    switch (format) {
        case ResourceFormat::eUndefined:
            return 0;
        case ResourceFormat::eRg4UNorm:
            return 1;
        case ResourceFormat::eRgba4UNorm:
        case ResourceFormat::eBgra4UNorm:
        case ResourceFormat::eRgb565UNorm:
        case ResourceFormat::eBgr565UNorm:
        case ResourceFormat::eRgb5A1UNorm:
        case ResourceFormat::eBgr5A1UNorm:
        case ResourceFormat::eA1Rgb5UNorm:
            return 2;
        case ResourceFormat::eR8UNorm:
        case ResourceFormat::eR8SNorm:
        case ResourceFormat::eR8UInt:
        case ResourceFormat::eR8SInt:
        case ResourceFormat::eR8Srgb:
            return 1;
        case ResourceFormat::eRg8UNorm:
        case ResourceFormat::eRg8SNorm:
        case ResourceFormat::eRg8UInt:
        case ResourceFormat::eRg8SInt:
        case ResourceFormat::eRg8Srgb:
            return 2;
        case ResourceFormat::eRgb8UNorm:
        case ResourceFormat::eRgb8SNorm:
        case ResourceFormat::eRgb8UInt:
        case ResourceFormat::eRgb8SInt:
        case ResourceFormat::eRgb8Srgb:
        case ResourceFormat::eBgr8UNorm:
        case ResourceFormat::eBgr8SNorm:
        case ResourceFormat::eBgr8UInt:
        case ResourceFormat::eBgr8SInt:
        case ResourceFormat::eBgr8Srgb:
            return 3;
        case ResourceFormat::eRgba8UNorm:
        case ResourceFormat::eRgba8SNorm:
        case ResourceFormat::eRgba8UInt:
        case ResourceFormat::eRgba8SInt:
        case ResourceFormat::eRgba8Srgb:
        case ResourceFormat::eBgra8UNorm:
        case ResourceFormat::eBgra8SNorm:
        case ResourceFormat::eBgra8UInt:
        case ResourceFormat::eBgra8SInt:
        case ResourceFormat::eBgra8Srgb:
        case ResourceFormat::eAbgr8UNorm:
        case ResourceFormat::eAbgr8SNorm:
        case ResourceFormat::eAbgr8UInt:
        case ResourceFormat::eAbgr8SInt:
        case ResourceFormat::eAbgr8Srgb:
        case ResourceFormat::eA2Rgb10UNorm:
        case ResourceFormat::eA2Rgb10SNorm:
        case ResourceFormat::eA2Rgb10UInt:
        case ResourceFormat::eA2Rgb10SInt:
        case ResourceFormat::eA2Bgr10UNorm:
        case ResourceFormat::eA2Bgr10SNorm:
        case ResourceFormat::eA2Brg10UInt:
        case ResourceFormat::eA2Brg10SInt:
            return 4;
        case ResourceFormat::eR16UNorm:
        case ResourceFormat::eR16SNorm:
        case ResourceFormat::eR16UInt:
        case ResourceFormat::eR16SInt:
        case ResourceFormat::eR16SFloat:
            return 2;
        case ResourceFormat::eRg16UNorm:
        case ResourceFormat::eRg16SNorm:
        case ResourceFormat::eRg16UInt:
        case ResourceFormat::eRg16SInt:
        case ResourceFormat::eRg16SFloat:
            return 4;
        case ResourceFormat::eRgb16UNorm:
        case ResourceFormat::eRgb16SNorm:
        case ResourceFormat::eRgb16UInt:
        case ResourceFormat::eRgb16SInt:
        case ResourceFormat::eRgb16SFloat:
            return 6;
        case ResourceFormat::eRgba16UNorm:
        case ResourceFormat::eRgba16SNorm:
        case ResourceFormat::eRgba16UInt:
        case ResourceFormat::eRgba16SInt:
        case ResourceFormat::eRgba16SFloat:
            return 8;
        case ResourceFormat::eR32UInt:
        case ResourceFormat::eR32SInt:
        case ResourceFormat::eR32SFloat:
            return 4;
        case ResourceFormat::eRg32UInt:
        case ResourceFormat::eRg32SInt:
        case ResourceFormat::eRg32SFloat:
            return 8;
        case ResourceFormat::eRgb32UInt:
        case ResourceFormat::eRgb32SInt:
        case ResourceFormat::eRgb32SFloat:
            return 12;
        case ResourceFormat::eRgba32UInt:
        case ResourceFormat::eRgba32SInt:
        case ResourceFormat::eRgba32SFloat:
            return 16;
        case ResourceFormat::eB10Gr11UFloat:
        case ResourceFormat::eE5Rgb9UFloat:
            return 4;
        case ResourceFormat::eD16UNorm:
        case ResourceFormat::eX8D24UNorm:
        case ResourceFormat::eD32SFloat:
        case ResourceFormat::eD16UNormS8UInt:
        case ResourceFormat::eD24UNormS8UInt:
        case ResourceFormat::eD32SFloatS8UInt:
        case ResourceFormat::eBc1RgbUNorm:
        case ResourceFormat::eBc1RgbSrgb:
        case ResourceFormat::eBc1RgbaUNorm:
        case ResourceFormat::eBc1RgbaSrgb:
        case ResourceFormat::eBc2UNorm:
        case ResourceFormat::eBc2Srgb:
        case ResourceFormat::eBc3UNorm:
        case ResourceFormat::eBc3Srgb:
        case ResourceFormat::eBc4UNorm:
        case ResourceFormat::eBc4SNorm:
        case ResourceFormat::eBc5UNorm:
        case ResourceFormat::eBc5SNorm:
        case ResourceFormat::eBc6HUFloat:
        case ResourceFormat::eBc6HSFLoat:
        case ResourceFormat::eBc7UNorm:
        case ResourceFormat::eBc7Srgb:
            return 0;
    }
    Unreachable();
}

inline VkCompareOp ToVkCompareOp(CompareOp op) {
    return static_cast<VkCompareOp>(op);
}

inline VkIndexType ToVkIndexType(IndexType ty) {
    return ty == IndexType::eUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

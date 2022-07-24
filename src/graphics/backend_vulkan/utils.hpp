#pragma once

#include <volk.h>

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

inline VkCompareOp ToVkCompareOp(CompareOp op) {
    return static_cast<VkCompareOp>(op);
}

inline VkIndexType ToVkIndexType(IndexType ty) {
    return ty == IndexType::eUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

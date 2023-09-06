#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/rhi/defines.hpp>

namespace bi::rhi {

struct VkStructHeader {
    VkStructureType sType;
    VkStructHeader *pNext;
};
inline void connect_vk_p_next_chain(VkStructHeader *struct1, VkStructHeader *struct2) {
    auto p = struct1;
    while (p->pNext != nullptr) { p = p->pNext; }
    p->pNext = struct2;
}
template <typename Struct1, typename Struct2>
void connect_vk_p_next_chain(Struct1 *struct1, Struct2 *struct2) {
    connect_vk_p_next_chain(reinterpret_cast<VkStructHeader *>(struct1), reinterpret_cast<VkStructHeader *>(struct2));
}

inline auto to_vk_format(ResourceFormat format) -> VkFormat {
    return static_cast<VkFormat>(format);
}
inline auto from_vk_format(VkFormat format) -> ResourceFormat {
    return static_cast<ResourceFormat>(format);
}

inline auto to_vk_compare_op(CompareOp op) -> VkCompareOp {
    return static_cast<VkCompareOp>(op);
}

inline auto to_vk_index_type(IndexType ty) -> VkIndexType {
    return ty == IndexType::uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}

}

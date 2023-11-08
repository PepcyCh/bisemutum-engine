#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/prelude/bitflags.hpp>
#include <bisemutum/rhi/defines.hpp>
#include <bisemutum/rhi/shader.hpp>

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

inline auto to_vk_shader_stages(BitFlags<ShaderStage> stage) -> VkShaderStageFlags {
    VkShaderStageFlags ret = 0;
    if (stage.contains_any(ShaderStage::vertex)) { ret |= VK_SHADER_STAGE_VERTEX_BIT; }
    if (stage.contains_any(ShaderStage::tessellation_control)) { ret |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; }
    if (stage.contains_any(ShaderStage::tessellation_evaluation)) { ret |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; }
    if (stage.contains_any(ShaderStage::geometry)) { ret |= VK_SHADER_STAGE_GEOMETRY_BIT; }
    if (stage.contains_any(ShaderStage::fragment)) { ret |= VK_SHADER_STAGE_FRAGMENT_BIT; }
    if (stage.contains_any(ShaderStage::compute)) { ret |= VK_SHADER_STAGE_COMPUTE_BIT; }
    if (stage.contains_any(ShaderStage::ray_generation)) { ret |= VK_SHADER_STAGE_RAYGEN_BIT_KHR; }
    if (stage.contains_any(ShaderStage::ray_miss)) { ret |= VK_SHADER_STAGE_MISS_BIT_KHR; }
    if (stage.contains_any(ShaderStage::ray_closest_hit)) { ret |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; }
    if (stage.contains_any(ShaderStage::ray_any_hit)) { ret |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR; }
    if (stage.contains_any(ShaderStage::ray_intersection)) { ret |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR; }
    if (stage.contains_any(ShaderStage::ray_callable)) { ret |= VK_SHADER_STAGE_CALLABLE_BIT_KHR; }
    if (stage.contains_any(ShaderStage::task)) { ret |= VK_SHADER_STAGE_TASK_BIT_EXT; }
    if (stage.contains_any(ShaderStage::mesh)) { ret |= VK_SHADER_STAGE_MESH_BIT_EXT; }
    return ret;
}

}

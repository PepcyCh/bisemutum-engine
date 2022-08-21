#include "descriptor.hpp"

#include <format>

#include "device.hpp"
#include "utils.hpp"
#include "resource.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VkDescriptorType ToVkDescriptorType(DescriptorType type) {
    switch (type) {
        case DescriptorType::eNone:
            Unreachable();
        case DescriptorType::eSampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::eUniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::eStorageBuffer:
        case DescriptorType::eRWStorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::eSampledTexture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::eStorageTexture:
        case DescriptorType::eRWStorageTexture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    Unreachable();
}

VkImageViewType ToVkImageViewType(TextureViewDimension dim) {
    switch (dim) {
        case TextureViewDimension::e1D: return VK_IMAGE_VIEW_TYPE_1D;
        case TextureViewDimension::e1DArray: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case TextureViewDimension::e2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureViewDimension::e2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureViewDimension::eCube: return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureViewDimension::eCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case TextureViewDimension::e3D: return VK_IMAGE_VIEW_TYPE_3D;
    }
    Unreachable();
}

VkDescriptorBufferInfo ToVkBufferInfo(const BufferRange &buffer) {
    return VkDescriptorBufferInfo {
        .buffer = buffer.buffer.CastTo<BufferVulkan>()->Raw(),
        .offset = buffer.offset,
        .range = buffer.length,
    };
}

VkDescriptorImageInfo ToVkImageInfo(const TextureView &texture, const DescriptorSetLayoutBinding &binding) {
    const auto texture_vk = texture.texture.CastTo<TextureVulkan>();
    TextureViewVulkanDesc view_desc {
        .type = ToVkImageViewType(binding.tex_dim),
        .format = binding.tex_format == ResourceFormat::eUndefined
            ? texture_vk->RawFormat() : ToVkFormat(binding.tex_format),
        .base_layer = texture.base_layer,
        .layers = texture.layers,
        .base_level = texture.base_level,
        .levels = texture.levels,
    };
    VkImageView view = texture_vk->GetView(view_desc);
    return VkDescriptorImageInfo {
        .sampler = VK_NULL_HANDLE,
        .imageView = view,
        .imageLayout = binding.type == DescriptorType::eSampledTexture
            ? (IsDepthStencilFormat(texture_vk->Desc().format)
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            : VK_IMAGE_LAYOUT_GENERAL,
    };
}

VkDescriptorImageInfo ToVkImageInfo(Ref<Sampler> sampler) {
    return VkDescriptorImageInfo {
        .sampler = sampler.CastTo<SamplerVulkan>()->Raw(),
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
}

}

constexpr DescriptorPoolSizesVulkan DescriptorPoolSizesVulkan::kDefault = {
    .sizes = {
        { .type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1024 },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 2048 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 2048 },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 2048 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2048 },
    },
    .num_sets = 1024,
};

DescriptorSetPoolVulkan::DescriptorSetPoolVulkan(Ref<DeviceVulkan> device, const DescriptorPoolSizesVulkan &max_sizes)
    : device_(device) {
    VkDescriptorPoolCreateInfo pool_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = max_sizes.num_sets,
        .poolSizeCount = static_cast<uint32_t>(DescriptorPoolSizesVulkan::kDescriptorSizesCount),
        .pPoolSizes = max_sizes.sizes,
    };
    vkCreateDescriptorPool(device->Raw(), &pool_ci, nullptr, &pool_);
}

DescriptorSetPoolVulkan::~DescriptorSetPoolVulkan() {
    vkDestroyDescriptorPool(device_->Raw(), pool_, nullptr);
}

VkDescriptorSet DescriptorSetPoolVulkan::AllocateAndWriteSet(VkDescriptorSetLayout layout_vk,
    const DescriptorSetLayout &layout, const ShaderParams &values) {
    VkDescriptorSetAllocateInfo set_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout_vk,
    };
    VkDescriptorSet set;
    vkAllocateDescriptorSets(device_->Raw(), &set_ci, &set);

    uint32_t num_buffers = 0;
    uint32_t num_textures = 0;
    uint32_t num_samplers = 0;
    for (const auto &binding : layout.bindings) {
        if (IsDescriptorTypeBuffer(binding.type)) {
            num_buffers += binding.count;
        } else if (IsDescriptorTypeTexture(binding.type)) {
            num_textures += binding.count;
        } else if (IsDescriptorTypeSampler(binding.type)) {
            num_samplers += binding.count;
        }
    }
    uint32_t total = num_buffers + num_textures + num_samplers;

    Vec<VkDescriptorBufferInfo> buffer_infos(num_buffers);
    VkDescriptorBufferInfo *p_buffer_info = buffer_infos.data();
    Vec<VkDescriptorImageInfo> image_infos(num_textures + num_samplers);
    VkDescriptorImageInfo *p_image_info = image_infos.data();
    Vec<VkWriteDescriptorSet> writes(layout.bindings.size());
    VkWriteDescriptorSet *p_write = writes.data();
    for (uint32_t binding = 0; binding < values.resources.size(); binding++) {
        const auto &resource = values.resources[binding];
        resource.Match(
            [](std::monostate) {},
            [set, binding, &layout, &p_write, &p_buffer_info](const BufferRange &buffer) {
                *p_buffer_info = ToVkBufferInfo(buffer);
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set,
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = ToVkDescriptorType(layout.bindings[binding].type),
                    .pImageInfo = nullptr,
                    .pBufferInfo = p_buffer_info,
                    .pTexelBufferView = nullptr,
                };
                ++p_buffer_info;
                ++p_write;
            },
            [set, binding, &layout, &p_write, &p_image_info](const TextureView &texture) {
                *p_image_info = ToVkImageInfo(texture, layout.bindings[binding]);
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set,
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = ToVkDescriptorType(layout.bindings[binding].type),
                    .pImageInfo = p_image_info,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                };
                ++p_image_info;
                ++p_write;
            },
            [set, binding, &layout, &p_write, &p_image_info](Ref<Sampler> sampler) {
                *p_image_info = ToVkImageInfo(sampler);
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set,
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = ToVkDescriptorType(layout.bindings[binding].type),
                    .pImageInfo = p_image_info,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                };
                ++p_image_info;
                ++p_write;
            },
            [set, binding, &layout, &p_write, &p_buffer_info](const Vec<BufferRange> &buffers) {
                auto p_buffer_info_start = p_buffer_info;
                for (const auto &buffer : buffers) {
                    *p_buffer_info = ToVkBufferInfo(buffer);
                    ++p_buffer_info;
                }
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set,
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = layout.bindings[binding].count,
                    .descriptorType = ToVkDescriptorType(layout.bindings[binding].type),
                    .pImageInfo = nullptr,
                    .pBufferInfo = p_buffer_info_start,
                    .pTexelBufferView = nullptr,
                };
                ++p_write;
            },
            [set, binding, &layout, &p_write, &p_image_info](const Vec<TextureView> &textures) {
                auto p_image_info_start = p_image_info;
                for (const auto &texture : textures) {
                    *p_image_info =
                        ToVkImageInfo(texture, layout.bindings[binding]);
                    ++p_image_info;
                }
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set,
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = layout.bindings[binding].count,
                    .descriptorType = ToVkDescriptorType(layout.bindings[binding].type),
                    .pImageInfo = p_image_info_start,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                };
                ++p_write;
            },
            [set, binding, &layout, &p_write, &p_image_info](const Vec<Ref<Sampler>> &samplers) {
                auto p_image_info_start = p_image_info;
                for (const auto &sampler : samplers) {
                    *p_image_info = ToVkImageInfo(sampler);
                    ++p_image_info;
                }
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set,
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = layout.bindings[binding].count,
                    .descriptorType = ToVkDescriptorType(layout.bindings[binding].type),
                    .pImageInfo = p_image_info_start,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                };
                ++p_write;
            }
        );
    }
    vkUpdateDescriptorSets(device_->Raw(), writes.size(), writes.data(), 0, nullptr);

    return set;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

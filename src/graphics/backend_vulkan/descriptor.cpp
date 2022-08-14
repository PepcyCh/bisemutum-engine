#include "descriptor.hpp"

#include <format>

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

bool IsBufferDescriptor(VkDescriptorType type) {
    return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
}

bool IsTextureDescriptor(VkDescriptorType type) {
    return type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
}

bool IsSamplerDescriptor(VkDescriptorType type) {
    return type == VK_DESCRIPTOR_TYPE_SAMPLER;
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


DescriptorSetVulkan::DescriptorSetVulkan(Ref<DeviceVulkan> device, VkDescriptorSet set,
    const ShaderInfoVulkan &shader_info, uint32_t set_index)
    : device_(device), set_(set), shader_info_(shader_info), set_index_(set_index) {
    binding_resources_.resize(shader_info.bindings.bindings[set_index].size(), std::monostate {});
}

void DescriptorSetVulkan::BindBuffer(uint32_t binding_index, const BufferRange &buffer) {
    if (const auto resource = std::get_if<BufferRange>(&binding_resources_[binding_index]);
        resource == nullptr || *resource == buffer) {
        BI_ASSERT_MSG(IsBufferDescriptor(shader_info_.bindings.bindings[set_index_][binding_index].descriptorType),
            std::format("Try to bind buffer to resource at {}.{}, which is not", set_index_, binding_index));
        binding_resources_[binding_index] = buffer;
        if (dirty_bindings_.insert(binding_index).second) {
            ++num_dirty_buffers_;
        }
    }
}

void DescriptorSetVulkan::BindTexture(uint32_t binding_index, const TextureRange &texture) {
    if (const auto resource = std::get_if<TextureRange>(&binding_resources_[binding_index]);
        resource == nullptr || *resource == texture) {
        BI_ASSERT_MSG(IsTextureDescriptor(shader_info_.bindings.bindings[set_index_][binding_index].descriptorType),
            std::format("Try to bind texture to resource at {}.{}, which is not", set_index_, binding_index));
        binding_resources_[binding_index] = texture;
        dirty_bindings_.insert(binding_index);
        if (dirty_bindings_.insert(binding_index).second) {
            ++num_dirty_textures_;
        }
    }
}

void DescriptorSetVulkan::BindSampler(uint32_t binding_index, Ref<Sampler> sampler) {
    if (const auto resource = std::get_if<Ref<Sampler>>(&binding_resources_[binding_index]);
        resource == nullptr || *resource == sampler) {
        BI_ASSERT_MSG(IsSamplerDescriptor(shader_info_.bindings.bindings[set_index_][binding_index].descriptorType),
            std::format("Try to bind sampler to resource at {}.{}, which is not", set_index_, binding_index));
        binding_resources_[binding_index] = sampler;
        dirty_bindings_.insert(binding_index);
        if (dirty_bindings_.insert(binding_index).second) {
            ++num_dirty_samplers_;
        }
    }
}

void DescriptorSetVulkan::Update() {
    if (!dirty_bindings_.empty()) {
        Vec<VkDescriptorBufferInfo> buffer_infos(num_dirty_buffers_);
        VkDescriptorBufferInfo *p_buffer_info = buffer_infos.data();
        Vec<VkDescriptorImageInfo> image_infos(num_dirty_textures_ + num_dirty_samplers_);
        VkDescriptorImageInfo *p_image_info = image_infos.data();
        Vec<VkWriteDescriptorSet> writes(dirty_bindings_.size());
        VkWriteDescriptorSet *p_write = writes.data();;
        for (uint32_t drity_binding : dirty_bindings_) {
            if (const auto buffer = std::get_if<BufferRange>(&binding_resources_[drity_binding]); buffer) {
                *p_buffer_info = VkDescriptorBufferInfo {
                    .buffer = buffer->buffer.CastTo<BufferVulkan>()->Raw(),
                    .offset = buffer->offset,
                    .range = buffer->length,
                };
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set_,
                    .dstBinding = drity_binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = shader_info_.bindings.bindings[set_index_][drity_binding].descriptorType,
                    .pImageInfo = nullptr,
                    .pBufferInfo = p_buffer_info,
                    .pTexelBufferView = nullptr,
                };
                ++p_buffer_info;
                ++p_write;
            } else if (const auto texture = std::get_if<TextureRange>(&binding_resources_[drity_binding]); texture) {
                const auto texture_vk = texture->texture.CastTo<TextureVulkan>();
                TextureViewVulkanDesc view_desc {
                    .type = shader_info_.bindings.bindings_view_info[set_index_][drity_binding].image_view_type,
                    .base_layer = texture->base_layer,
                    .layers = texture->layers,
                    .base_level = texture->base_level,
                    .levels = texture->levels,
                };
                VkImageView view = texture_vk->GetView(view_desc);
                *p_image_info = VkDescriptorImageInfo {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = view,
                    .imageLayout = texture_vk->Layout(),
                };
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set_,
                    .dstBinding = drity_binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = shader_info_.bindings.bindings[set_index_][drity_binding].descriptorType,
                    .pImageInfo = p_image_info,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                };
                ++p_image_info;
                ++p_write;
            } else if (const auto sampler = std::get_if<Ref<Sampler>>(&binding_resources_[drity_binding]); sampler) {
                *p_image_info = VkDescriptorImageInfo {
                    .sampler = sampler->CastTo<SamplerVulkan>()->Raw(),
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                };
                *p_write = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = set_,
                    .dstBinding = drity_binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = shader_info_.bindings.bindings[set_index_][drity_binding].descriptorType,
                    .pImageInfo = p_image_info,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr,
                };
                ++p_image_info;
                ++p_write;
            }
        }

        vkUpdateDescriptorSets(device_->Raw(), writes.size(), writes.data(), 0, nullptr);

        dirty_bindings_.clear();
        num_dirty_buffers_ = 0;
        num_dirty_textures_ = 0;
        num_dirty_samplers_ = 0;
    }
}


DescriptorPoolVulkan::DescriptorPoolVulkan(Ref<DeviceVulkan> device, const DescriptorPoolSizesVulkan &max_sizes)
    : device_(device) {
    VkDescriptorPoolCreateInfo pool_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = max_sizes.num_sets,
        .poolSizeCount = DescriptorPoolSizesVulkan::kDescriptorSizesCount,
        .pPoolSizes = max_sizes.sizes,
    };
}

DescriptorPoolVulkan::~DescriptorPoolVulkan() {
    vkDestroyDescriptorPool(device_->Raw(), pool_, nullptr);
}

DescriptorSetVulkan DescriptorPoolVulkan::AllocateSet(RenderPipeline *pipeline, uint32_t set_index) {
    auto pipeline_vk = static_cast<RenderPipelineVulkan *>(pipeline);
    auto layout = pipeline_vk->RawSetLayout(set_index);
    VkDescriptorSetAllocateInfo set_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkDescriptorSet set;
    vkAllocateDescriptorSets(device_->Raw(), &set_ci, &set);

    return DescriptorSetVulkan(device_, set, pipeline_vk->ShaderInfo(), set_index);
}

DescriptorSetVulkan DescriptorPoolVulkan::AllocateSet(ComputePipeline *pipeline, uint32_t set_index) {
    auto pipeline_vk = static_cast<ComputePipelineVulkan *>(pipeline);
    auto layout = pipeline_vk->RawSetLayout(set_index);
    VkDescriptorSetAllocateInfo set_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkDescriptorSet set;
    vkAllocateDescriptorSets(device_->Raw(), &set_ci, &set);

    return DescriptorSetVulkan(device_, set, pipeline_vk->ShaderInfo(), set_index);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

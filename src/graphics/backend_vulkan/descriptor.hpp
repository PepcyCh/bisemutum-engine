#pragma once

#include <variant>

#include "core/ptr.hpp"
#include "graphics/mod.hpp"
#include "pipeline.hpp"
#include "resource.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct DescriptorPoolSizesVulkan {
    static constexpr size_t kDescriptorSizesCount = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER + 1;

    static const DescriptorPoolSizesVulkan kDefault;

    VkDescriptorPoolSize sizes[kDescriptorSizesCount];
    uint32_t num_sets = 0;
};

class DescriptorSetVulkan {
public:
    DescriptorSetVulkan(Ref<class DeviceVulkan> device, VkDescriptorSet set, const ShaderInfoVulkan &shader_info,
        uint32_t set_index);

    void BindBuffer(uint32_t binding_index, const BufferRange &buffer);
    void BindTexture(uint32_t binding_index, const TextureRange &texture);
    void BindSampler(uint32_t binding_index, Ref<Sampler> sampler);

    void Update();
    
    VkDescriptorSet Raw() const { return set_; }

private:
    Ref<DeviceVulkan> device_;
    VkDescriptorSet set_;

    const ShaderInfoVulkan &shader_info_;
    uint32_t set_index_;

    using BindingResource = std::variant<std::monostate, BufferRange, TextureRange, Ref<Sampler>>;
    Vec<BindingResource> binding_resources_;

    HashSet<uint32_t> dirty_bindings_;
    uint32_t num_dirty_buffers_ = 0;
    uint32_t num_dirty_textures_ = 0;
    uint32_t num_dirty_samplers_ = 0;
};

class DescriptorPoolVulkan {
public:
    DescriptorPoolVulkan(Ref<class DeviceVulkan> device, const DescriptorPoolSizesVulkan &max_sizes);
    ~DescriptorPoolVulkan();

    DescriptorSetVulkan AllocateSet(RenderPipeline *pipeline, uint32_t set_index);
    DescriptorSetVulkan AllocateSet(ComputePipeline *pipeline, uint32_t set_index);

private:
    Ref<DeviceVulkan> device_;
    VkDescriptorPool pool_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

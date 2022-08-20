#pragma once

#include <volk.h>

#include "graphics/descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct DescriptorPoolSizesVulkan {
    static constexpr size_t kDescriptorSizesCount = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER + 1;

    static const DescriptorPoolSizesVulkan kDefault;

    VkDescriptorPoolSize sizes[kDescriptorSizesCount];
    uint32_t num_sets = 0;
};

class DescriptorSetPoolVulkan {
public:
    DescriptorSetPoolVulkan(Ref<class DeviceVulkan> device,
        const DescriptorPoolSizesVulkan &max_sizes = DescriptorPoolSizesVulkan::kDefault);
    ~DescriptorSetPoolVulkan();

    VkDescriptorSet AllocateAndWriteSet(VkDescriptorSetLayout layout_vk, const DescriptorSetLayout &layout,
        const ShaderParams &values);

private:
    Ref<DeviceVulkan> device_;
    VkDescriptorPool pool_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

#include "sampler.hpp"

#include "volk.h"
#include "utils.hpp"
#include "device.hpp"

namespace bi::rhi {

namespace {

auto to_vk_filter_mode(SamplerFilterMode filter) -> VkFilter {
    return static_cast<VkFilter>(filter);
}

auto to_vk_mipmap_mode(SamplerMipmapMode mipmap) -> VkSamplerMipmapMode {
    return static_cast<VkSamplerMipmapMode>(mipmap);
}

auto to_vk_address_mode(SamplerAddressMode address) -> VkSamplerAddressMode {
    return static_cast<VkSamplerAddressMode>(address);
}

auto to_vk_border_color(SamplerBorderColor border) -> VkBorderColor {
    return static_cast<VkBorderColor>(border);
}

}

SamplerVulkan::SamplerVulkan(Ref<DeviceVulkan> device, SamplerDesc const& desc) : device_(device) {
    desc_ = desc;
    VkSamplerCreateInfo sampler_ci {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = to_vk_filter_mode(desc.mag_filter),
        .minFilter = to_vk_filter_mode(desc.min_filter),
        .mipmapMode = to_vk_mipmap_mode(desc.mipmap_mode),
        .addressModeU = to_vk_address_mode(desc.address_mode_u),
        .addressModeV = to_vk_address_mode(desc.address_mode_v),
        .addressModeW = to_vk_address_mode(desc.address_mode_w),
        .mipLodBias = desc.lod_bias,
        .anisotropyEnable = desc.anisotropy > 0.0f,
        .maxAnisotropy = desc.anisotropy,
        .compareEnable = desc.compare_enabled,
        .compareOp = to_vk_compare_op(desc.compare_op),
        .minLod = desc.lod_min,
        .maxLod = desc.lod_max,
        .borderColor = to_vk_border_color(desc.border_color),
        .unnormalizedCoordinates = VK_FALSE,
    };

    vkCreateSampler(device_->raw(), &sampler_ci, nullptr, &sampler_);
}

SamplerVulkan::~SamplerVulkan() {
    if (sampler_) {
        vkDestroySampler(device_->raw(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
}

}

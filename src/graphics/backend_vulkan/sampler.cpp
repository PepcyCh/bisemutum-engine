#include "sampler.hpp"

#include "utils.hpp"
#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VkFilter ToVkFilterMode(SamplerFilterMode filter) {
    return static_cast<VkFilter>(filter);
}

VkSamplerMipmapMode ToVkMipmapMode(SamplerMipmapMode mipmap) {
    return static_cast<VkSamplerMipmapMode>(mipmap);
}

VkSamplerAddressMode ToVkAddressMode(SamplerAddressMode address) {
    return static_cast<VkSamplerAddressMode>(address);
}

VkBorderColor ToVkBorderColor(SamplerBorderColor border) {
    return static_cast<VkBorderColor>(border);
}

}

SamplerVulkan::SamplerVulkan(DeviceVulkan *device, const SamplerDesc &desc) {
    device_ = device;

    VkSamplerCreateInfo sampler_ci {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = ToVkFilterMode(desc.mag_filter),
        .minFilter = ToVkFilterMode(desc.min_filter),
        .mipmapMode = ToVkMipmapMode(desc.mipmap_mode),
        .addressModeU = ToVkAddressMode(desc.address_mode_u),
        .addressModeV = ToVkAddressMode(desc.address_mode_v),
        .addressModeW = ToVkAddressMode(desc.address_mode_w),
        .mipLodBias = desc.lod_bias,
        .anisotropyEnable = desc.anisotropy > 0.0f,
        .maxAnisotropy = desc.anisotropy,
        .compareEnable = desc.compare_enabled,
        .compareOp = ToVkCompareOp(desc.compare_op),
        .minLod = desc.lod_min,
        .maxLod = desc.lod_max,
        .borderColor = ToVkBorderColor(desc.border_color),
        .unnormalizedCoordinates = VK_FALSE,
    };

    vkCreateSampler(device_->Raw(), &sampler_ci, nullptr, &sampler_);
}

SamplerVulkan::~SamplerVulkan() {
    vkDestroySampler(device_->Raw(), sampler_, nullptr);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

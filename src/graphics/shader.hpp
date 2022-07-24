#pragma once

#include "resource.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct DescriptorBinding {
    uint32_t set : 8 = 0;
    uint32_t binding : 24 = 0;
};

enum class DescriptorType : uint8_t {
    eSampler,
    eUniformBuffer,
    eStorageBuffer,
    eAccelerationStructure,
    eSampledTexture,
    eStorageTexture,
};

struct DescriptorLayout {
    std::unordered_map<uint8_t, std::unordered_map<uint32_t, DescriptorType>> descriptors;
    uint32_t push_constant_length = 0;
};

class ShaderModule {
public:
    virtual ~ShaderModule() = default;

protected:
    ShaderModule(uint32_t *spv_data, uint64_t length);

    DescriptorLayout layout_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

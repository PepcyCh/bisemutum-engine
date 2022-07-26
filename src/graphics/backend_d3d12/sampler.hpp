#pragma once

#include "core/ptr.hpp"
#include "graphics/sampler.hpp"
#include "utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class SamplerD3D12 : public Sampler {
public:
    SamplerD3D12(Ref<class DeviceD3D12> device, const SamplerDesc &desc);
    ~SamplerD3D12() override;

private:
    Ref<DeviceD3D12> device_;
    D3D12_SAMPLER_DESC sampler_desc_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

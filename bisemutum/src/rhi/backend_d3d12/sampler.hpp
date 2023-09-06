#pragma once

#include <bisemutum/rhi/sampler.hpp>
#include <bisemutum/prelude/ref.hpp>
#include <d3d12.h>

namespace bi::rhi {

struct SamplerD3D12 final : Sampler {
    SamplerD3D12(Ref<struct DeviceD3D12> device, SamplerDesc const& desc);

    auto sampler_desc() const -> D3D12_SAMPLER_DESC const& { return sampler_desc_; }

    auto static_border_color() const -> D3D12_STATIC_BORDER_COLOR { return border_color_; }

private:
    Ref<DeviceD3D12> device_;

    D3D12_SAMPLER_DESC sampler_desc_;
    D3D12_STATIC_BORDER_COLOR border_color_;
};

}

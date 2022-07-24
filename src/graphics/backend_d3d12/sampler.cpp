#include "sampler.hpp"

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

D3D12_FILTER ToDxFilterMode(SamplerFilterMode min, SamplerFilterMode mag, SamplerMipmapMode mipmap) {
    UINT filter = 0;

    switch (mipmap) {
        case SamplerMipmapMode::eNearest:
            filter |= (0x0 << 0);
            break;
        case SamplerMipmapMode::eLinear:
            filter |= (0x1 << 0);
            break;
    }
    switch (mag) {
        case SamplerFilterMode::eNearest:
            filter |= (0x0 << 2);
            break;
        case SamplerFilterMode::eLinear:
            filter |= (0x1 << 2);
            break;
    }
    switch (min) {
        case SamplerFilterMode::eNearest:
            filter |= (0x0 << 4);
            break;
        case SamplerFilterMode::eLinear:
            filter |= (0x1 << 4);
            break;
    }

    return static_cast<D3D12_FILTER>(filter);
}

D3D12_TEXTURE_ADDRESS_MODE ToDxAddressMode(SamplerAddressMode address) {
    switch (address) {
        case SamplerAddressMode::eRepeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerAddressMode::eMirrorRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerAddressMode::eClampToEdge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerAddressMode::eClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerAddressMode::eMirrorClampToEdge: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    }
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

void ToDxBorderColor(SamplerBorderColor border, float color[4]) {
    switch (border) {
        case SamplerBorderColor::TransparentFloat:
        case SamplerBorderColor::TransparentInt:
            color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
            color[3] = 0.0f;
            break;
        case SamplerBorderColor::BlackFloat:
        case SamplerBorderColor::BlackInt:
            color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
            color[3] = 1.0f;
            break;
        case SamplerBorderColor::WhiteFloat:
        case SamplerBorderColor::WhiteInt:
            color[0] = 1.0f;
            color[1] = 1.0f;
            color[2] = 1.0f;
            color[3] = 1.0f;
            break;
    }
}

}

SamplerD3D12::SamplerD3D12(DeviceD3D12 *device, const SamplerDesc &desc) {
    device_ = device;

    sampler_desc_ = D3D12_SAMPLER_DESC {
        .Filter = ToDxFilterMode(desc.min_filter, desc.mag_filter, desc.mipmap_mode),
        .AddressU = ToDxAddressMode(desc.address_mode_u),
        .AddressV = ToDxAddressMode(desc.address_mode_v),
        .AddressW = ToDxAddressMode(desc.address_mode_w),
        .MipLODBias = desc.lod_bias,
        .MaxAnisotropy = static_cast<UINT>(desc.anisotropy),
        .ComparisonFunc = desc.compare_enabled ? ToDxCompareFunc(desc.compare_op) : D3D12_COMPARISON_FUNC_NEVER,
        .MinLOD = desc.lod_min,
        .MaxLOD = desc.lod_max,
    };
    ToDxBorderColor(desc.border_color, sampler_desc_.BorderColor);
}

SamplerD3D12::~SamplerD3D12() {}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

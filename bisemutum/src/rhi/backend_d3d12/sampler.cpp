#include "sampler.hpp"

#include "device.hpp"
#include "utils.hpp"

namespace bi::rhi {

namespace {

auto to_dx_filter_mode(SamplerFilterMode min, SamplerFilterMode mag, SamplerMipmapMode mipmap) -> D3D12_FILTER {
    UINT filter = 0;

    switch (mipmap) {
        case SamplerMipmapMode::nearest:
            filter |= (0x0 << 0);
            break;
        case SamplerMipmapMode::linear:
            filter |= (0x1 << 0);
            break;
    }
    switch (mag) {
        case SamplerFilterMode::nearest:
            filter |= (0x0 << 2);
            break;
        case SamplerFilterMode::linear:
            filter |= (0x1 << 2);
            break;
    }
    switch (min) {
        case SamplerFilterMode::nearest:
            filter |= (0x0 << 4);
            break;
        case SamplerFilterMode::linear:
            filter |= (0x1 << 4);
            break;
    }

    return static_cast<D3D12_FILTER>(filter);
}

auto to_dx_address_mode(SamplerAddressMode address) -> D3D12_TEXTURE_ADDRESS_MODE {
    switch (address) {
        case SamplerAddressMode::repeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerAddressMode::mirror_repeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerAddressMode::clamp_to_edge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerAddressMode::clamp_to_border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerAddressMode::mirror_clamp_to_edge: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    }
    unreachable();
}

void to_dx_border_color(SamplerBorderColor border, float color[4]) {
    switch (border) {
        case SamplerBorderColor::transparent_float:
        case SamplerBorderColor::transparent_int:
            color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
            color[3] = 0.0f;
            break;
        case SamplerBorderColor::black_float:
        case SamplerBorderColor::black_int:
            color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
            color[3] = 1.0f;
            break;
        case SamplerBorderColor::white_float:
        case SamplerBorderColor::white_int:
            color[0] = 1.0f;
            color[1] = 1.0f;
            color[2] = 1.0f;
            color[3] = 1.0f;
            break;
    }
}

auto to_dx_static_border_color(SamplerBorderColor border) -> D3D12_STATIC_BORDER_COLOR {
    switch (border) {
        case SamplerBorderColor::transparent_float:
        case SamplerBorderColor::transparent_int:
            return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        case SamplerBorderColor::black_float:
        case SamplerBorderColor::black_int:
            return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        case SamplerBorderColor::white_float:
        case SamplerBorderColor::white_int:
            return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    }
    unreachable();
}

}

SamplerD3D12::SamplerD3D12(Ref<DeviceD3D12> device, SamplerDesc const& desc) : device_(device) {
    sampler_desc_ = D3D12_SAMPLER_DESC{
        .Filter = to_dx_filter_mode(desc.min_filter, desc.mag_filter, desc.mipmap_mode),
        .AddressU = to_dx_address_mode(desc.address_mode_u),
        .AddressV = to_dx_address_mode(desc.address_mode_v),
        .AddressW = to_dx_address_mode(desc.address_mode_w),
        .MipLODBias = desc.lod_bias,
        .MaxAnisotropy = static_cast<UINT>(desc.anisotropy),
        .ComparisonFunc = desc.compare_enabled ? to_dx_compare_op(desc.compare_op) : D3D12_COMPARISON_FUNC_NEVER,
        .MinLOD = desc.lod_min,
        .MaxLOD = desc.lod_max,
    };
    to_dx_border_color(desc.border_color, sampler_desc_.BorderColor);

    border_color_ = to_dx_static_border_color(desc.border_color);
}

}

#pragma once

#include <bisemutum/rhi/defines.hpp>
#include <bisemutum/prelude/misc.hpp>

#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>

namespace bi::rhi {

inline auto to_dx_compare_op(CompareOp op) -> D3D12_COMPARISON_FUNC {
    return static_cast<D3D12_COMPARISON_FUNC>(static_cast<uint8_t>(op) + 1);
}

inline auto to_dx_format(ResourceFormat format) -> DXGI_FORMAT {
    switch (format) {
        case ResourceFormat::undefined: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::rg4_unorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::rgba4_unorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::bgra4_unorm: return DXGI_FORMAT_B4G4R4A4_UNORM;
        case ResourceFormat::rgb565_unorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::bgr565_unorm: return DXGI_FORMAT_B5G6R5_UNORM;
        case ResourceFormat::rgb5a1_unorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::bgr5a1_unorm: return DXGI_FORMAT_B5G5R5A1_UNORM;
        case ResourceFormat::a1rgb5_unorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::r8_unorm: return DXGI_FORMAT_R8_UNORM;
        case ResourceFormat::r8_snorm: return DXGI_FORMAT_R8_SNORM;
        case ResourceFormat::r8_uint: return DXGI_FORMAT_R8_UINT;
        case ResourceFormat::r8_sint: return DXGI_FORMAT_R8_SINT;
        case ResourceFormat::r8_srgb: return DXGI_FORMAT_R8_UNORM;
        case ResourceFormat::rg8_unorm: return DXGI_FORMAT_R8G8_UNORM;
        case ResourceFormat::rg8_snorm: return DXGI_FORMAT_R8G8_SNORM; 
        case ResourceFormat::rg8_uint: return DXGI_FORMAT_R8G8_UINT;
        case ResourceFormat::rg8_sint: return DXGI_FORMAT_R8G8_SINT;
        case ResourceFormat::rg8_srgb: return DXGI_FORMAT_R8G8_UNORM;
        case ResourceFormat::rgba8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case ResourceFormat::rgba8_snorm: return DXGI_FORMAT_R8G8B8A8_SNORM; 
        case ResourceFormat::rgba8_uint: return DXGI_FORMAT_R8G8B8A8_UINT;
        case ResourceFormat::rgba8_sint: return DXGI_FORMAT_R8G8B8A8_SINT;
        case ResourceFormat::rgba8_srgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case ResourceFormat::bgra8_unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case ResourceFormat::bgra8_snorm: return DXGI_FORMAT_B8G8R8A8_TYPELESS; 
        case ResourceFormat::bgra8_uint: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case ResourceFormat::bgra8_sint: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case ResourceFormat::bgra8_srgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case ResourceFormat::abgr8_unorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::abgr8_snorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::abgr8_uint: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::abgr8_sint: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::abgr8_srgb: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::a2rgb10_unorm: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case ResourceFormat::a2rgb10_snorm: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::a2rgb10_uint: return DXGI_FORMAT_R10G10B10A2_UINT;
        case ResourceFormat::a2rgb10_sint: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::a2bgr10_unorm: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::a2bgr10_snorm: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::a2bgr10_uint: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::a2bgr10_sint: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::r16_unorm: return DXGI_FORMAT_R16_UNORM;
        case ResourceFormat::r16_snorm: return DXGI_FORMAT_R16_SNORM;
        case ResourceFormat::r16_uint: return DXGI_FORMAT_R16_UINT;
        case ResourceFormat::r16_sint: return DXGI_FORMAT_R16_SINT;
        case ResourceFormat::r16_sfloat: return DXGI_FORMAT_R16_FLOAT;
        case ResourceFormat::rg16_unorm: return DXGI_FORMAT_R16G16_UNORM;
        case ResourceFormat::rg16_snorm: return DXGI_FORMAT_R16G16_SNORM;
        case ResourceFormat::rg16_uint: return DXGI_FORMAT_R16G16_UINT;
        case ResourceFormat::rg16_sint: return DXGI_FORMAT_R16G16_SINT;
        case ResourceFormat::rg16_sfloat: return DXGI_FORMAT_R16G16_FLOAT;
        case ResourceFormat::rgba16_unorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case ResourceFormat::rgba16_snorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case ResourceFormat::rgba16_uint: return DXGI_FORMAT_R16G16B16A16_UINT;
        case ResourceFormat::rgba16_sint: return DXGI_FORMAT_R16G16B16A16_SINT;
        case ResourceFormat::rgba16_sfloat: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case ResourceFormat::r32_uint: return DXGI_FORMAT_R32_UINT;
        case ResourceFormat::r32_sint: return DXGI_FORMAT_R32_SINT;
        case ResourceFormat::r32_sfloat: return DXGI_FORMAT_R32_FLOAT;
        case ResourceFormat::rg32_uint: return DXGI_FORMAT_R32G32_UINT;
        case ResourceFormat::rg32_sint: return DXGI_FORMAT_R32G32_SINT;
        case ResourceFormat::rg32_sfloat: return DXGI_FORMAT_R32G32_FLOAT;
        case ResourceFormat::rgb32_uint: return DXGI_FORMAT_R32G32B32_UINT;
        case ResourceFormat::rgb32_sint: return DXGI_FORMAT_R32G32B32_SINT;
        case ResourceFormat::rgb32_sfloat: return DXGI_FORMAT_R32G32B32_FLOAT;
        case ResourceFormat::rgba32_uint: return DXGI_FORMAT_R32G32B32A32_UINT;
        case ResourceFormat::rgba32_sint: return DXGI_FORMAT_R32G32B32A32_SINT;
        case ResourceFormat::rgba32_sfloat: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case ResourceFormat::b10gr11_ufloat: return DXGI_FORMAT_R11G11B10_FLOAT;
        case ResourceFormat::e5rgb9_ufloat: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case ResourceFormat::d16_unorm: return DXGI_FORMAT_D16_UNORM;
        case ResourceFormat::x8d24_unorm: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case ResourceFormat::d32_sfloat: return DXGI_FORMAT_D32_FLOAT;
        case ResourceFormat::d16_unorm_s8_uint:  return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::d24_unorm_s8_uint:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case ResourceFormat::d32_sfloat_s8_uint: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case ResourceFormat::bc1_rgb_unorm: return DXGI_FORMAT_BC1_UNORM;
        case ResourceFormat::bc1_rgb_srgb: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case ResourceFormat::bc1_rgba_unorm: return DXGI_FORMAT_BC1_UNORM;
        case ResourceFormat::bc1_rgba_srgb: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case ResourceFormat::bc2_unorm: return DXGI_FORMAT_BC2_UNORM;
        case ResourceFormat::bc2_srgb: return DXGI_FORMAT_BC2_UNORM_SRGB;
        case ResourceFormat::bc3_unorm: return DXGI_FORMAT_BC3_UNORM;
        case ResourceFormat::bc3_srgb: return DXGI_FORMAT_BC3_UNORM_SRGB;
        case ResourceFormat::bc4_unorm: return DXGI_FORMAT_BC4_UNORM;
        case ResourceFormat::bc4_snorm: return DXGI_FORMAT_BC4_SNORM;
        case ResourceFormat::bc5_unorm: return DXGI_FORMAT_BC5_UNORM;
        case ResourceFormat::bc5_snorm: return DXGI_FORMAT_BC5_SNORM;
        case ResourceFormat::bc6h_ufloat: return DXGI_FORMAT_BC6H_UF16;
        case ResourceFormat::bc6h_sfloat: return DXGI_FORMAT_BC6H_SF16;
        case ResourceFormat::bc7_unorm: return DXGI_FORMAT_BC7_UNORM;
        case ResourceFormat::bc7_sgrb: return DXGI_FORMAT_BC7_UNORM_SRGB;
    }
    unreachable();
}

inline auto to_dx_index_format(IndexType type) -> DXGI_FORMAT {
    return type == IndexType::uint16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
}

}

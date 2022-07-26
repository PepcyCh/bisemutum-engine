#pragma once

#include <string>

#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include "core/utils.hpp"
#include "graphics/defines.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

using Microsoft::WRL::ComPtr;

inline std::string WCharsToString(const WCHAR *wchars) {
    int size = WideCharToMultiByte(CP_OEMCP, 0, wchars, -1, nullptr, 0, nullptr, nullptr);
    CHAR *temp  = new CHAR[size];
    WideCharToMultiByte(CP_OEMCP, 0, wchars, -1, temp, size, nullptr, nullptr);
    std::string str(temp, temp + size - 1);
    delete[] temp;
    return str;
}

inline D3D12_COMPARISON_FUNC ToDxCompareFunc(CompareOp op) {
    return static_cast<D3D12_COMPARISON_FUNC>(static_cast<uint8_t>(op) + 1);
}

inline DXGI_FORMAT ToDxFormat(ResourceFormat format) {
    switch (format) {
        case ResourceFormat::eUndefined: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eRg4UNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eRgba4UNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eBgra4UNorm: return DXGI_FORMAT_B4G4R4A4_UNORM;
        case ResourceFormat::eRgb565UNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eBgr565UNorm: return DXGI_FORMAT_B5G6R5_UNORM;
        case ResourceFormat::eRgb5A1UNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eBgr5A1UNorm: return DXGI_FORMAT_B5G5R5A1_UNORM;
        case ResourceFormat::eA1Rgb5UNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eR8UNorm: return DXGI_FORMAT_R8_UNORM;
        case ResourceFormat::eR8SNorm: return DXGI_FORMAT_R8_SNORM;
        case ResourceFormat::eR8UInt: return DXGI_FORMAT_R8_UINT;
        case ResourceFormat::eR8SInt: return DXGI_FORMAT_R8_SINT;
        case ResourceFormat::eR8Srgb: return DXGI_FORMAT_R8_UNORM;
        case ResourceFormat::eRg8UNorm: return DXGI_FORMAT_R8G8_UNORM;
        case ResourceFormat::eRg8SNorm: return DXGI_FORMAT_R8G8_SNORM; 
        case ResourceFormat::eRg8UInt: return DXGI_FORMAT_R8G8_UINT;
        case ResourceFormat::eRg8SInt: return DXGI_FORMAT_R8G8_SINT;
        case ResourceFormat::eRg8Srgb: return DXGI_FORMAT_R8G8_UNORM;
        case ResourceFormat::eRgb8UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case ResourceFormat::eRgb8SNorm: return DXGI_FORMAT_R8G8B8A8_SNORM; 
        case ResourceFormat::eRgb8UInt: return DXGI_FORMAT_R8G8B8A8_UINT;
        case ResourceFormat::eRgb8SInt: return DXGI_FORMAT_R8G8B8A8_SINT;
        case ResourceFormat::eRgb8Srgb: return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case ResourceFormat::eBgr8UNorm: return DXGI_FORMAT_B8G8R8X8_UNORM;
        case ResourceFormat::eBgr8SNorm: return DXGI_FORMAT_B8G8R8X8_TYPELESS; 
        case ResourceFormat::eBgr8UInt: return DXGI_FORMAT_B8G8R8X8_TYPELESS;
        case ResourceFormat::eBgr8SInt: return DXGI_FORMAT_B8G8R8X8_TYPELESS;
        case ResourceFormat::eBgr8Srgb: return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case ResourceFormat::eRgba8UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case ResourceFormat::eRgba8SNorm: return DXGI_FORMAT_R8G8B8A8_SNORM; 
        case ResourceFormat::eRgba8UInt: return DXGI_FORMAT_R8G8B8A8_UINT;
        case ResourceFormat::eRgba8SInt: return DXGI_FORMAT_R8G8B8A8_SINT;
        case ResourceFormat::eRgba8Srgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case ResourceFormat::eBgra8UNorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case ResourceFormat::eBgra8SNorm: return DXGI_FORMAT_B8G8R8A8_TYPELESS; 
        case ResourceFormat::eBgra8UInt: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case ResourceFormat::eBgra8SInt: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case ResourceFormat::eBgra8Srgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case ResourceFormat::eAbgr8UNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eAbgr8SNorm: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eAbgr8UInt: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eAbgr8SInt: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eAbgr8Srgb: return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eA2Rgb10UNorm: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case ResourceFormat::eA2Rgb10SNorm: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::eA2Rgb10UInt: return DXGI_FORMAT_R10G10B10A2_UINT;
        case ResourceFormat::eA2Rgb10SInt: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::eA2Bgr10UNorm: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::eA2Bgr10SNorm: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::eA2Brg10UInt: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::eA2Brg10SInt: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case ResourceFormat::eR16UNorm: return DXGI_FORMAT_R16_UNORM;
        case ResourceFormat::eR16SNorm: return DXGI_FORMAT_R16_SNORM;
        case ResourceFormat::eR16UInt: return DXGI_FORMAT_R16_UINT;
        case ResourceFormat::eR16SInt: return DXGI_FORMAT_R16_SINT;
        case ResourceFormat::eR16SFloat: return DXGI_FORMAT_R16_FLOAT;
        case ResourceFormat::eRg16UNorm: return DXGI_FORMAT_R16G16_UNORM;
        case ResourceFormat::eRg16SNorm: return DXGI_FORMAT_R16G16_SNORM;
        case ResourceFormat::eRg16UInt: return DXGI_FORMAT_R16G16_UINT;
        case ResourceFormat::eRg16SInt: return DXGI_FORMAT_R16G16_SINT;
        case ResourceFormat::eRg16SFloat: return DXGI_FORMAT_R16G16_FLOAT;
        case ResourceFormat::eRgb16UNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case ResourceFormat::eRgb16SNorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case ResourceFormat::eRgb16UInt: return DXGI_FORMAT_R16G16B16A16_UINT;
        case ResourceFormat::eRgb16SInt: return DXGI_FORMAT_R16G16B16A16_SINT;
        case ResourceFormat::eRgb16SFloat: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case ResourceFormat::eRgba16UNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case ResourceFormat::eRgba16SNorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case ResourceFormat::eRgba16UInt: return DXGI_FORMAT_R16G16B16A16_UINT;
        case ResourceFormat::eRgba16SInt: return DXGI_FORMAT_R16G16B16A16_SINT;
        case ResourceFormat::eRgba16SFloat: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case ResourceFormat::eR32UInt: return DXGI_FORMAT_R32_UINT;
        case ResourceFormat::eR32SInt: return DXGI_FORMAT_R32_SINT;
        case ResourceFormat::eR32SFloat: return DXGI_FORMAT_R32_FLOAT;
        case ResourceFormat::eRg32UInt: return DXGI_FORMAT_R32G32_UINT;
        case ResourceFormat::eRg32SInt: return DXGI_FORMAT_R32G32_SINT;
        case ResourceFormat::eRg32SFloat: return DXGI_FORMAT_R32G32_FLOAT;
        case ResourceFormat::eRgb32UInt: return DXGI_FORMAT_R32G32B32_UINT;
        case ResourceFormat::eRgb32SInt: return DXGI_FORMAT_R32G32B32_SINT;
        case ResourceFormat::eRgb32SFloat: return DXGI_FORMAT_R32G32B32_FLOAT;
        case ResourceFormat::eRgba32UInt: return DXGI_FORMAT_R32G32B32A32_UINT;
        case ResourceFormat::eRgba32SInt: return DXGI_FORMAT_R32G32B32A32_SINT;
        case ResourceFormat::eRgba32SFloat: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case ResourceFormat::eB10Gr11UFloat: return DXGI_FORMAT_R11G11B10_FLOAT;
        case ResourceFormat::eE5Rgb9UFloat: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case ResourceFormat::eD16UNorm: return DXGI_FORMAT_D16_UNORM;
        case ResourceFormat::eX8D24UNorm: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case ResourceFormat::eD32SFloat: return DXGI_FORMAT_D32_FLOAT;
        case ResourceFormat::eD16UNormS8UInt:  return DXGI_FORMAT_UNKNOWN;
        case ResourceFormat::eD24UNormS8UInt:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case ResourceFormat::eD32SFloatS8UInt: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case ResourceFormat::eBc1RgbUNorm: return DXGI_FORMAT_BC1_UNORM;
        case ResourceFormat::eBc1RgbSrgb: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case ResourceFormat::eBc1RgbaUNorm: return DXGI_FORMAT_BC1_UNORM;
        case ResourceFormat::eBc1RgbaSrgb: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case ResourceFormat::eBc2UNorm: return DXGI_FORMAT_BC2_UNORM;
        case ResourceFormat::eBc2Srgb: return DXGI_FORMAT_BC2_UNORM_SRGB;
        case ResourceFormat::eBc3UNorm: return DXGI_FORMAT_BC3_UNORM;
        case ResourceFormat::eBc3Srgb: return DXGI_FORMAT_BC3_UNORM_SRGB;
        case ResourceFormat::eBc4UNorm: return DXGI_FORMAT_BC4_UNORM;
        case ResourceFormat::eBc4SNorm: return DXGI_FORMAT_BC4_SNORM;
        case ResourceFormat::eBc5UNorm: return DXGI_FORMAT_BC5_UNORM;
        case ResourceFormat::eBc5SNorm: return DXGI_FORMAT_BC5_SNORM;
        case ResourceFormat::eBc6HUFloat: return DXGI_FORMAT_BC6H_UF16;
        case ResourceFormat::eBc6HSFLoat: return DXGI_FORMAT_BC6H_SF16;
        case ResourceFormat::eBc7UNorm: return DXGI_FORMAT_BC7_UNORM;
        case ResourceFormat::eBc7Srgb: return DXGI_FORMAT_BC7_UNORM_SRGB;
    }
    Unreachable();
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

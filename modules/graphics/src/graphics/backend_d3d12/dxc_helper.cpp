#include "dxc_helper.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

DxcHelper &DxcHelper::Instance() {
    static DxcHelper gDxcHelper {};
    return gDxcHelper;
}

DxcHelper::DxcHelper() {
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
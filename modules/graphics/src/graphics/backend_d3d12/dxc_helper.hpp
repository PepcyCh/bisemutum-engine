#pragma once

#include "utils.hpp" // dxcapi.h needs defination of IUnknown...
#include <dxcapi.h>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DxcHelper {
public:
    static DxcHelper &Instance();

    IDxcUtils *Utils() const { return utils_.Get(); }

private:
    DxcHelper();
    
    ComPtr<IDxcUtils> utils_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

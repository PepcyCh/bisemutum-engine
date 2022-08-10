#pragma once

#include <cstdint>

#include "mod.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class ShaderStage : uint8_t {
    eVertex,
    eTessellationControl,
    eTessellationEvaluation,
    eGeometry,
    eFragment,
    eCompute,
};

class ShaderModule {
public:
    virtual ~ShaderModule() = default;

protected:
    ShaderModule() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

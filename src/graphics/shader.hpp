#pragma once

#include "core/ptr.hpp"
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

struct GraphicsProgramDesc {
    Ref<ShaderModule> vertex;
    ShaderModule *tessellation_control = nullptr;
    ShaderModule *tessellation_evaluation = nullptr;
    ShaderModule *geometry = nullptr;
    Ref<ShaderModule> fragment;
};

class GraphicsProgram {
public:
    virtual ~GraphicsProgram() = default;

protected:
    GraphicsProgram() = default;
};

struct ComputeProgramDesc {
    Ref<ShaderModule> compute;
};

class ComputeProgram {
public:
    virtual ~ComputeProgram() = default;

protected:
    ComputeProgram() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

#pragma once

#include "core/bitflags.hpp"
#include "core/container.hpp"
#include "core/ptr.hpp"
#include "defines.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class VertexSemantics : uint8_t {
    ePosition,
    eColor,
    eNormal,
    eTangent,
    eBitangent,
    eTexcoord0,
    eTexcoord1,
    eTexcoord2,
    eTexcoord3,
    eTexcoord4,
    eTexcoord5,
    eTexcoord6,
    eTexcoord7,
};
struct VertexInputAttribute {
    uint32_t offset;
    VertexSemantics semantics;
};
struct VertexInputBufferDesc {
    uint32_t stride = 0;
    bool per_instance = 0;
    Vec<VertexInputAttribute> attributes;
};

enum class PrimitiveTopology : uint8_t {
    ePointList,
    eLineList,
    eLineStrip,
    eTriangleList,
    eTriangleStrip,
    eTriangleFan,
    eLineListAdjacency,
    eLineStripAdjacency,
    eTriangleListAdjacency,
    eTriangleStripAdjacency,
    ePatchList,
};
enum class FrontFace : uint8_t {
    eCcw,
    eCw,
};
enum class CullMode : uint8_t {
    eNone,
    eBackFace,
    eFrontFace,
};
enum class PolygonMode : uint8_t {
    eFill,
    eLine,
    ePoint,
};
struct PrimitiveState {
    PrimitiveTopology topology = PrimitiveTopology::eTriangleList;
    FrontFace front_face = FrontFace::eCcw;
    CullMode cull_mode = CullMode::eNone;
    PolygonMode polygon_mode = PolygonMode::eFill;
    bool conservative = false;
};

enum class StencilOp : uint8_t {
    eKeep,
    eZero,
    eReplace,
    eIncrementClamp,
    eDecrementClamp,
    eInvert,
    eIncrementWrap,
    eDecrementWrap,
};
struct StencilFaceState {
    CompareOp compare_op = CompareOp::eNever;
    StencilOp fail_op = StencilOp::eKeep;
    StencilOp pass_op = StencilOp::eKeep;
    StencilOp depth_fail_op = StencilOp::eKeep;
};
struct DepthStencilState {
    ResourceFormat format = ResourceFormat::eUndefined;
    bool depth_write : 1 = true;
    bool depth_test : 1 = true;
    bool stencil_test : 1 = false;
    CompareOp depth_compare_op = CompareOp::eLess;
    StencilFaceState stencil_front_face;
    StencilFaceState stencil_back_face;
    uint8_t stencil_compare_mask = 0xff;
    uint8_t stencil_write_mask = 0xff;
    uint8_t stencil_reference = 0;
};

enum class BlendOp : uint8_t {
    eAdd,
    eSubtract,
    eReserveSubtract,
    eMin,
    eMax,
};
enum class BlendFactor : uint8_t {
    eZero,
    eOne,
    eSrc,
    eOneMinusSrc,
    eSrcAlpha,
    eOneMinusSrcAlpha,
    eDst,
    eOneMinusDst,
    eDstAlpha,
    eOneMinusDstAlpha,
    eSrcAlphaSaturated,
    eConstant,
    eOneMinusConstant,
};
enum class ColorWriteComponent : uint8_t {
    eR = 0x1,
    eG = 0x2,
    eB = 0x4,
    eA = 0x8,
    eRgb = eR | eG | eB,
    eRgba = eR | eG | eB | eA,
};
struct ColorTargetAttachmentState {
    ResourceFormat format = ResourceFormat::eUndefined;
    bool blend_enable = false;
    BlendOp blend_op = BlendOp::eAdd;
    BlendFactor src_blend_factor = BlendFactor::eOne;
    BlendFactor dst_blend_factor = BlendFactor::eZero;
    BlendOp alpha_blend_op = BlendOp::eAdd;
    BlendFactor src_alpha_blend_factor = BlendFactor::eOne;
    BlendFactor dst_alpha_blend_factor = BlendFactor::eZero;
    BitFlags<ColorWriteComponent> color_write_mask = { ColorWriteComponent::eRgba };
};
struct ColorTargetState {
    Vec<ColorTargetAttachmentState> attachments;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    } blend_constants;
};

struct RenderPipelineDesc {
    Vec<VertexInputBufferDesc> vertex_input_buffers;
    PrimitiveState primitive_state;
    DepthStencilState depth_stencil_state;
    ColorTargetState color_target_state;
    
    struct {
        Ref<ShaderModule> vertex;
        ShaderModule *tessellation_control = nullptr;
        ShaderModule *tessellation_evaluation = nullptr;
        ShaderModule *geometry = nullptr;
        Ref<ShaderModule> fragment;
    } shaders;
};

class RenderPipeline {
public:
    virtual ~RenderPipeline() = default;

protected:
    RenderPipeline() = default;
};

struct ComputePipelineDesc {
    Ref<ShaderModule> compute;
};

class ComputePipeline {
public:
    virtual ~ComputePipeline() = default;

protected:
    ComputePipeline() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

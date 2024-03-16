#pragma once

#include "defines.hpp"
#include "descriptor.hpp"

namespace bi::rhi {

enum class VertexSemantics : uint8_t {
    position,
    color,
    normal,
    tangent,
    bitangent,
    texcoord0,
    texcoord1,
    texcoord2,
    texcoord3,
    texcoord4,
    texcoord5,
    texcoord6,
    texcoord7,
};
struct VertexInputAttribute final {
    uint32_t offset = 0;
    VertexSemantics semantics = VertexSemantics::position;
    ResourceFormat format = ResourceFormat::undefined;
};
struct VertexInputBufferDesc final {
    uint32_t stride = 0;
    bool per_instance = false;
    std::vector<VertexInputAttribute> attributes;
};

enum class PrimitiveTopology : uint8_t {
    point_list,
    line_list,
    line_strip,
    triangle_list,
    triangle_strip,
    line_list_adjacency,
    line_strip_adjacency,
    triangle_list_adjacency,
    triangle_strip_adjacency,
    patch_list,
};
enum class FrontFace : uint8_t {
    ccw,
    cw,
};
enum class CullMode : uint8_t {
    none,
    back_face,
    front_face,
};
enum class PolygonMode : uint8_t {
    fill,
    line,
    point,
};
struct RasterizationState final {
    PrimitiveTopology topology = PrimitiveTopology::triangle_list;
    FrontFace front_face = FrontFace::ccw;
    CullMode cull_mode = CullMode::none;
    PolygonMode polygon_mode = PolygonMode::fill;
    bool conservative = false;
};

struct TessellationState final {

};

enum class StencilOp : uint8_t {
    keep,
    zero,
    replace,
    increment_clamp,
    decrement_clamp,
    invert,
    increment_wrap,
    decrement_wrap,
};
struct StencilFaceState final {
    CompareOp compare_op = CompareOp::never;
    StencilOp fail_op = StencilOp::keep;
    StencilOp pass_op = StencilOp::keep;
    StencilOp depth_fail_op = StencilOp::keep;
};
struct DepthStencilState final {
    ResourceFormat format = ResourceFormat::undefined;
    bool depth_write : 1 = true;
    bool depth_test : 1 = true;
    bool stencil_test : 1 = false;
    CompareOp depth_compare_op = CompareOp::less;
    StencilFaceState stencil_front_face;
    StencilFaceState stencil_back_face;
    uint8_t stencil_compare_mask = 0xff;
    uint8_t stencil_write_mask = 0xff;
    uint8_t stencil_reference = 0;
};

enum class BlendOp : uint8_t {
    add,
    subtract,
    reverse_subtract,
    min,
    max,
};
enum class BlendFactor : uint8_t {
    zero,
    one,
    src,
    one_minus_src,
    src_alpha,
    one_minus_src_alpha,
    dst,
    one_minus_dst,
    dst_alpha,
    one_minus_dst_alpha,
    src_alpha_saturated,
    constant,
    one_minus_constant,
};
enum class ColorWriteComponent : uint8_t {
    r = 0x1,
    g = 0x2,
    b = 0x4,
    a = 0x8,
    rgb = r | g | b,
    rgba = r | g | b | a,
};
struct ColorTargetAttachmentState final {
    ResourceFormat format = ResourceFormat::undefined;
    bool blend_enable = false;
    BlendOp blend_op = BlendOp::add;
    BlendFactor src_blend_factor = BlendFactor::one;
    BlendFactor dst_blend_factor = BlendFactor::zero;
    BlendOp alpha_blend_op = BlendOp::add;
    BlendFactor src_alpha_blend_factor = BlendFactor::one;
    BlendFactor dst_alpha_blend_factor = BlendFactor::zero;
    BitFlags<ColorWriteComponent> color_write_mask = {ColorWriteComponent::rgba};
};
struct ColorTargetState final {
    std::vector<ColorTargetAttachmentState> attachments;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    } blend_constants;
};

struct PipelineShader final {
    CRef<ShaderModule> shader_module;
    std::string_view entry;
};

struct PushConstantsDesc final {
    uint32_t size = 0;
    BitFlags<ShaderStage> visibility = ShaderStage::all_stages;
    uint32_t register_ = 0;
    uint32_t space = 0;
};

struct GraphicsPipelineDesc final {
    std::vector<BindGroupLayout> bind_groups_layout;
    std::vector<StaticSampler> static_samplers;
    PushConstantsDesc push_constants;

    std::vector<VertexInputBufferDesc> vertex_input_buffers;
    TessellationState tessellation_state;
    RasterizationState rasterization_state;
    DepthStencilState depth_stencil_state;
    ColorTargetState color_target_state;

    struct {
        PipelineShader vertex;
        Option<PipelineShader> tessellation_control;
        Option<PipelineShader> tessellation_evaluation;
        Option<PipelineShader> geometry;
        PipelineShader fragment;
    } shaders;
};

struct GraphicsPipeline {
    GraphicsPipeline(GraphicsPipelineDesc const& desc) : desc_(desc) {}
    virtual ~GraphicsPipeline() = default;

    auto desc() const -> GraphicsPipelineDesc const& { return desc_; }

protected:
    GraphicsPipelineDesc desc_;
};

struct MeshletPipelineDesc final {
    std::vector<BindGroupLayout> bind_groups_layout;
    std::vector<StaticSampler> static_samplers;
    PushConstantsDesc push_constants;

    RasterizationState rasterization_state;
    DepthStencilState depth_stencil_state;
    ColorTargetState color_target_state;

    struct {
        Option<PipelineShader> task;
        PipelineShader mesh;
        PipelineShader fragment;
    } shaders;
};

// TODO - support meshlet pipeline
struct MeshletPipeline {
    MeshletPipeline(MeshletPipelineDesc const& desc) : desc_(desc) {}
    virtual ~MeshletPipeline() = default;

    auto desc() const -> MeshletPipelineDesc const& { return desc_; }

protected:
    MeshletPipelineDesc desc_;
};

struct ComputePipelineDesc final {
    std::vector<BindGroupLayout> bind_groups_layout;
    std::vector<StaticSampler> static_samplers;
    PushConstantsDesc push_constants;

    PipelineShader compute;
};

struct ComputePipeline {
    ComputePipeline(ComputePipelineDesc const& desc) : desc_(desc) {}
    virtual ~ComputePipeline() = default;

    auto desc() const -> ComputePipelineDesc const& { return desc_; }

protected:
    ComputePipelineDesc desc_;
};

struct RaytracingState final {
    uint32_t max_recursive_depth = 1;
    bool skip_procedural = false;
    uint32_t max_ray_payload_size = 256;
    uint32_t max_hit_attribute_size = 2 * sizeof(float);
};

// Currently, only one single root constant (size is in bytes) is supported as LRS in D3D12.
// Because it is hard to write more complex LRS that is compatible with Vulkan SBT record.
struct RaytracingShaderRecordSizes final {
    uint32_t raygen = 0;
    uint32_t miss = 0;
    uint32_t hit_group = 0;
    uint32_t callable = 0;
};

struct RaytracingShaderGeneralGroup final {
    PipelineShader shader;
};
struct RaytracingShaderHitGroup final {
    Option<PipelineShader> closest_hit;
    Option<PipelineShader> any_hit;
    Option<PipelineShader> intersection;
};

struct RaytracingPipelineDesc final {
    std::vector<BindGroupLayout> bind_groups_layout;
    std::vector<StaticSampler> static_samplers;
    PushConstantsDesc push_constants;

    RaytracingState state;
    RaytracingShaderRecordSizes shader_record_sizes;

    struct {
        RaytracingShaderGeneralGroup raygen;
        std::vector<RaytracingShaderGeneralGroup> miss;
        std::vector<RaytracingShaderHitGroup> hit_group;
        std::vector<RaytracingShaderGeneralGroup> callable;
    } shaders;
};

struct RaytracingShaderBindingTableSizes final {
    uint32_t raygen_size = 0;
    uint32_t miss_stride = 0;
    uint32_t hit_group_stride = 0;
    uint32_t callable_stride = 0;
};

struct RaytracingShaderBindingTableRequirements final {
    uint32_t handle_size = 0;
    uint32_t handle_alignment = 0;
    uint32_t base_alignment = 0;
};

enum class RaytracingShaderBindingTableType : uint8_t {
    raygen,
    miss,
    hit_group,
    callable,
};

inline constexpr uint32_t d3d12_local_root_signature_space = 8;
inline constexpr uint32_t d3d12_local_root_signature_register_raygen = 0;
inline constexpr uint32_t d3d12_local_root_signature_register_miss = 1;
inline constexpr uint32_t d3d12_local_root_signature_register_hit_group = 2;
inline constexpr uint32_t d3d12_local_root_signature_register_callable = 3;

struct RaytracingPipeline {
    RaytracingPipeline(RaytracingPipelineDesc const& desc) : desc_(desc) {}
    virtual ~RaytracingPipeline() = default;

    auto desc() const -> RaytracingPipelineDesc const& { return desc_; }

    virtual auto get_shader_binding_table_sizes() const -> RaytracingShaderBindingTableSizes = 0;

    virtual auto get_shader_handle(
        RaytracingShaderBindingTableType type, uint32_t from_index, uint32_t count, void* dst_data
    ) const -> void = 0;

protected:
    RaytracingPipelineDesc desc_;
};

} // namespace bi::rhi

template <>
struct std::hash<bi::rhi::BindGroupLayoutEntry> final {
    auto operator()(bi::rhi::BindGroupLayoutEntry const& v) const noexcept -> size_t;
};

template <>
struct std::hash<bi::rhi::BindGroupLayout> final {
    auto operator()(bi::rhi::BindGroupLayout const& v) const noexcept -> size_t;
};

template <>
struct std::hash<bi::rhi::StaticSampler> final {
    auto operator()(bi::rhi::StaticSampler const& v) const noexcept -> size_t;
};

template <>
struct std::hash<bi::rhi::PushConstantsDesc> final {
    auto operator()(bi::rhi::PushConstantsDesc const& v) const noexcept -> size_t;
};

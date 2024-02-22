#pragma once

#include "resource.hpp"
#include "accel.hpp"
#include "descriptor.hpp"
#include "../prelude/box.hpp"
#include "../prelude/span.hpp"

namespace bi::rhi {

struct Queue;

struct CommandPoolDesc final {
    CRef<Queue> queue;
};

struct CommandPool {
    virtual ~CommandPool() = default;

    virtual auto reset() -> void = 0;

    virtual auto get_command_encoder() -> Box<struct CommandEncoder> = 0;
};

struct CommandLabel final {
    std::string_view label;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 1.0f;
    } color;
};

struct BufferCopyDesc final {
    uint64_t src_offset = 0;
    uint64_t dst_offset = 0;
    uint64_t length = ~0ull;
};

struct TextureCopyDesc final {
    Offset3D src_offset = {};
    Offset3D dst_offset = {};
    Extent3D extent = {~0u, ~0u, 1};
    uint32_t src_level = 0;
    uint32_t src_layer = 0;
    uint32_t dst_level = 0;
    uint32_t dst_layer = 0;
};

struct BufferTextureCopyDesc final {
    uint64_t buffer_offset = 0;
    uint32_t buffer_pixels_per_row = 0;
    uint32_t buffer_rows_per_texture = 0;
    Offset3D texture_offset = {0, 0, 0};
    Extent3D texture_extent = {~0u, ~0u, ~0u};
    uint32_t texture_level = 0;
    uint32_t texture_layer = 0;
};

struct ColorAttachmentDesc final {
    RenderTargetTextureDesc texture;
    Option<RenderTargetTextureDesc> resolve = {};
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    } clear_color;
    bool clear = false;
    bool store = true;
};
struct DepthStencilAttachmentDesc final {
    RenderTargetTextureDesc texture;
    float clear_depth = 1.0f;
    uint8_t clear_stencil = 0;
    bool clear = false;
    bool store = true;
};
inline constexpr uint32_t max_num_render_targets = 8;
struct RenderTargetDesc final {
    std::vector<ColorAttachmentDesc> colors = {};
    Option<DepthStencilAttachmentDesc> depth_stencil = {};
};

struct BufferBarrier final {
    Ref<Buffer> buffer;
    BitFlags<ResourceAccessType> src_access_type = {};
    BitFlags<ResourceAccessType> dst_access_type = {};
    Option<CRef<Queue>> src_queue = {};
    Option<CRef<Queue>> dst_queue = {};
};
struct TextureBarrier final {
    Ref<Texture> texture;
    uint32_t base_level = 0;
    uint32_t num_levels = ~0u;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;
    BitFlags<ResourceAccessType> src_access_type = {};
    BitFlags<ResourceAccessType> dst_access_type = {};
    Option<CRef<Queue>> src_queue = {};
    Option<CRef<Queue>> dst_queue = {};
};

struct Viewport final {
    float x = 0.0f;
    float y = 0.0f;
    float width;
    float height;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};
struct Scissor final {
    int x = 0;
    int y = 0;
    uint32_t width;
    uint32_t height;
};

struct CommandEncoder;
struct GraphicsCommandEncoder;
struct ComputeCommandEncoder;
struct RaytracingCommandEncoder;

struct CommandBuffer {
    virtual ~CommandBuffer() = default;
};

struct CommandEncoderBase {
    virtual ~CommandEncoderBase() = default;

    virtual auto push_label(CommandLabel const& label) -> void = 0;
    virtual auto pop_label() -> void = 0;
};

struct CommandEncoder : public CommandEncoderBase {
    virtual ~CommandEncoder() = default;

    virtual auto finish() -> Box<CommandBuffer> = 0;

    virtual auto copy_buffer_to_buffer(
        CRef<Buffer> src_buffer,
        Ref<Buffer> dst_buffer,
        BufferCopyDesc const& region
    ) -> void = 0;

    virtual auto copy_texture_to_texture(
        CRef<Texture> src_texture,
        Ref<Texture> dst_texture,
        TextureCopyDesc const& region
    ) -> void = 0;

    virtual auto copy_buffer_to_texture(
        CRef<Buffer> src_buffer,
        Ref<Texture> dst_texture,
        BufferTextureCopyDesc const& region
    ) -> void = 0;

    virtual auto copy_texture_to_buffer(
        CRef<Texture> src_texture,
        Ref<Buffer> dst_buffer,
        BufferTextureCopyDesc const& region
    ) -> void = 0;

    virtual auto build_bottom_level_acceleration_structure(
        CSpan<AccelerationStructureGeometryBuildDesc> build_infos
    ) -> void = 0;

    virtual auto build_top_level_acceleration_structure(
        CSpan<AccelerationStructureInstanceBuildDesc> build_infos
    ) -> void = 0;

    virtual auto copy_acceleration_structure(
        CRef<AccelerationStructure> src_acceleration_structure,
        Ref<AccelerationStructure> dst_acceleration_structure
    ) -> void = 0;

    virtual auto compact_acceleration_structure(
        CRef<AccelerationStructure> src_acceleration_structure,
        Ref<AccelerationStructure> dst_acceleration_structure
    ) -> void = 0;

    virtual auto resource_barriers(
        CSpan<BufferBarrier> buffer_barriers,
        CSpan<TextureBarrier> texture_barriers
    ) -> void = 0;

    virtual auto set_descriptor_heaps(CSpan<Ref<DescriptorHeap>> heaps) -> void = 0;

    virtual auto begin_render_pass(
        CommandLabel const& label,
        RenderTargetDesc const& desc
    ) -> Box<GraphicsCommandEncoder> = 0;

    virtual auto begin_compute_pass(CommandLabel const& label) -> Box<ComputeCommandEncoder> = 0;

    virtual auto begin_raytracing_pass(CommandLabel const& label) -> Box<RaytracingCommandEncoder> = 0;

    // If it is in `{Graphics|Compute|Raytracing}CommandEncoder`, then it is invalid, otherwise valid.
    virtual auto valid() const -> bool = 0;
};

struct GraphicsCommandEncoder : public CommandEncoderBase {
    virtual ~GraphicsCommandEncoder() = default;

    virtual auto set_pipeline(CRef<struct GraphicsPipeline> pipeline) -> void = 0;

    virtual auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void = 0;
    virtual auto push_constants(void const* data, uint32_t size, uint32_t offset = 0) -> void = 0;

    virtual auto set_viewports(CSpan<Viewport> viewports) -> void = 0;
    virtual auto set_scissors(CSpan<Scissor> scissors) -> void = 0;

    virtual auto set_vertex_buffer(
        CSpan<Ref<Buffer>> buffers, CSpan<uint64_t> offsets = {}, uint32_t first_binding = 0
    ) -> void = 0;
    virtual auto set_index_buffer(
        Ref<Buffer> buffer, uint64_t offset = 0, IndexType index_type = IndexType::uint32
    ) -> void = 0;

    virtual auto draw(
        uint32_t num_vertices,
        uint32_t num_instance = 1,
        uint32_t first_vertex = 0,
        uint32_t first_instance = 0
    ) -> void = 0;
    virtual auto draw_indexed(
        uint32_t num_indices,
        uint32_t num_instance = 1,
        uint32_t first_index = 0,
        uint32_t vertex_offset = 0,
        uint32_t first_instance = 0
    ) -> void = 0;
};

struct ComputeCommandEncoder : public CommandEncoderBase {
    virtual ~ComputeCommandEncoder() = default;

    virtual void set_pipeline(CRef<struct ComputePipeline> pipeline) = 0;

    virtual auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void = 0;
    virtual auto push_constants(void const* data, uint32_t size, uint32_t offset = 0) -> void = 0;

    virtual auto dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) -> void = 0;
};

struct RaytracingShaderBindingTableBuffers final {
    CPtr<Buffer> raygen_buffer;
    CPtr<Buffer> miss_buffer;
    CPtr<Buffer> hit_group_buffer;
    CPtr<Buffer> callable_buffer;
    uint32_t raygen_offset = 0;
    uint32_t miss_offset = 0;
    uint32_t hit_group_offset = 0;
    uint32_t callable_offset = 0;
};
struct RaytracingCommandEncoder : public CommandEncoderBase {
    virtual ~RaytracingCommandEncoder() = default;

    virtual void set_pipeline(CRef<struct RaytracingPipeline> pipeline) = 0;

    virtual auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void = 0;
    virtual auto push_constants(void const* data, uint32_t size, uint32_t offset = 0) -> void = 0;

    virtual auto dispatch_rays(
        RaytracingShaderBindingTableBuffers const& sbt, uint32_t width, uint32_t height = 1, uint32_t depth = 1
    ) -> void = 0;
};

}

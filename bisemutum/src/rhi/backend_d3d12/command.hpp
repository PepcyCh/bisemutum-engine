#pragma once

#include <bisemutum/rhi/command.hpp>
#include <d3d12.h>
#include <wrl.h>

namespace bi::rhi {

struct DeviceD3D12;

struct CommandPoolD3D12 final : CommandPool {
    CommandPoolD3D12(Ref<DeviceD3D12> device, CommandPoolDesc const& desc);

    auto reset() -> void override;

    auto get_command_encoder() -> Box<CommandEncoder> override;

private:
    Ref<DeviceD3D12> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmd_allocator_;

    std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>> allocated_cmd_lists_;
    size_t available_cmd_list_index_ = 0;
};

struct CommandBufferD3D12 final : CommandBuffer {
    CommandBufferD3D12(Ref<DeviceD3D12> device, ID3D12GraphicsCommandList4* cmd_list);

    auto raw() const -> ID3D12GraphicsCommandList4* { return cmd_list_; }

private:
    Ref<DeviceD3D12> device_;
    ID3D12GraphicsCommandList4* cmd_list_ = nullptr;
};

struct GraphicsCommandEncoderD3D12;
struct ComputeCommandEncoderD3D12;
struct RaytracingCommandEncoderD3D12;

struct CommandEncoderD3D12 final : CommandEncoder {
    CommandEncoderD3D12(Ref<DeviceD3D12> device, ID3D12GraphicsCommandList4* cmd_list);
    ~CommandEncoderD3D12();

    auto finish() -> Box<CommandBuffer> override;

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    auto copy_buffer_to_buffer(
        CRef<Buffer> src_buffer,
        Ref<Buffer> dst_buffer,
        BufferCopyDesc const& region
    ) -> void override;

    auto copy_texture_to_texture(
        CRef<Texture> src_texture,
        Ref<Texture> dst_texture,
        TextureCopyDesc const& region
    ) -> void override;

    auto copy_buffer_to_texture(
        CRef<Buffer> src_buffer,
        Ref<Texture> dst_texture,
        BufferTextureCopyDesc const& region
    ) -> void override;

    auto copy_texture_to_buffer(
        CRef<Texture> src_texture,
        Ref<Buffer> dst_buffer,
        BufferTextureCopyDesc const& region
    ) -> void override;

    auto build_bottom_level_acceleration_structure(
        CSpan<AccelerationStructureGeometryBuildDesc> build_infos
    ) -> void override;

    auto build_top_level_acceleration_structure(
        AccelerationStructureInstanceBuildDesc const& build_info
    ) -> void override;

    auto copy_acceleration_structure(
        CRef<AccelerationStructure> src_acceleration_structure,
        Ref<AccelerationStructure> dst_acceleration_structure
    ) -> void override;

    auto compact_acceleration_structure(
        CRef<AccelerationStructure> src_acceleration_structure,
        Ref<AccelerationStructure> dst_acceleration_structure
    ) -> void override;

    auto resource_barriers(
        CSpan<BufferBarrier> buffer_barriers, CSpan<TextureBarrier> texture_barriers
    ) -> void override;

    auto set_descriptor_heaps(CSpan<Ref<DescriptorHeap>> heaps) -> void override;

    auto begin_render_pass(
        CommandLabel const& label, RenderTargetDesc const& desc
    ) -> Box<GraphicsCommandEncoder> override;

    auto begin_compute_pass(CommandLabel const& label) -> Box<ComputeCommandEncoder> override;

    auto begin_raytracing_pass(CommandLabel const& label) -> Box<RaytracingCommandEncoder> override;

    auto valid() const -> bool override { return cmd_list_ != nullptr; }

private:
    friend GraphicsCommandEncoderD3D12;
    friend ComputeCommandEncoderD3D12;
    friend RaytracingCommandEncoderD3D12;

    Ref<DeviceD3D12> device_;
    ID3D12GraphicsCommandList4* cmd_list_;
};

struct GraphicsCommandEncoderD3D12 final : GraphicsCommandEncoder {
    GraphicsCommandEncoderD3D12(
        Ref<DeviceD3D12> device,
        RenderTargetDesc const& desc,
        Ref<CommandEncoderD3D12> base_encoder,
        bool has_label
    );
    ~GraphicsCommandEncoderD3D12();

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    auto set_pipeline(CRef<GraphicsPipeline> pipeline) -> void override;

    auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void override;

    auto push_constants(void const* data, uint32_t size, uint32_t offset) -> void override;

    void set_viewports(CSpan<Viewport> viewports) override;
    void set_scissors(CSpan<Scissor> scissors) override;

    auto set_vertex_buffer(
        CSpan<Ref<Buffer>> buffers, CSpan<uint64_t> offsets, uint32_t first_binding
    ) -> void override;
    auto set_index_buffer(
        Ref<Buffer> buffer, uint64_t offset, IndexType index_type
    ) -> void override;

    auto draw(
        uint32_t num_vertices,
        uint32_t num_instance,
        uint32_t first_vertex,
        uint32_t first_instance
    ) -> void override;
    auto draw_indexed(
        uint32_t num_indices,
        uint32_t num_instance,
        uint32_t first_index,
        uint32_t vertex_offset,
        uint32_t first_instance
    ) -> void override;

private:
    Ref<DeviceD3D12> device_;
    Ref<CommandEncoderD3D12> base_encoder_;
    ID3D12GraphicsCommandList4* cmd_list_;
    bool has_label_;

    struct GraphicsPipelineD3D12 const* curr_pipeline_ = nullptr;
};

struct ComputeCommandEncoderD3D12 final : ComputeCommandEncoder {
    ComputeCommandEncoderD3D12(Ref<DeviceD3D12> device, Ref<CommandEncoderD3D12> base_encoder, bool has_label);
    ~ComputeCommandEncoderD3D12();

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    auto set_pipeline(CRef<ComputePipeline> pipeline) -> void override;

    auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void override;

    auto push_constants(void const* data, uint32_t size, uint32_t offset) -> void override;

    void dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) override;

private:
    Ref<DeviceD3D12> device_;
    Ref<CommandEncoderD3D12> base_encoder_;
    ID3D12GraphicsCommandList4* cmd_list_;
    bool has_label_;

    struct ComputePipelineD3D12 const* curr_pipeline_ = nullptr;
};

struct RaytracingCommandEncoderD3D12 : public RaytracingCommandEncoder {
    RaytracingCommandEncoderD3D12(Ref<DeviceD3D12> device, Ref<CommandEncoderD3D12> base_encoder, bool has_label);
    ~RaytracingCommandEncoderD3D12();

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    void set_pipeline(CRef<RaytracingPipeline> pipeline) override;

    auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void override;

    auto push_constants(void const* data, uint32_t size, uint32_t offset = 0) -> void override;

    auto dispatch_rays(
        RaytracingShaderBindingTableBuffers const& sbt, uint32_t width, uint32_t height, uint32_t depth
    ) -> void override;

private:
    Ref<DeviceD3D12> device_;
    Ref<CommandEncoderD3D12> base_encoder_;
    ID3D12GraphicsCommandList4* cmd_list_;
    bool has_label_;

    struct RaytracingPipelineD3D12 const* curr_pipeline_ = nullptr;
};

}

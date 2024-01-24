#pragma once

#include <array>

#include <bisemutum/rhi/device.hpp>
#include <bisemutum/prelude/hash.hpp>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <D3D12MemAlloc.h>
#include <wrl.h>

#ifdef interface
#undef interface
#endif

#include "descriptor.hpp"
#include "pipeline_cache.hpp"

namespace bi::rhi {

struct DeviceD3D12 final : Device {
    DeviceD3D12(DeviceDesc const& desc);
    ~DeviceD3D12() override;

    static Box<DeviceD3D12> create(DeviceDesc const& desc);

    auto get_backend() const -> Backend override { return Backend::d3d12; }

    auto raytracing_shader_binding_table_requirements() const -> RaytracingShaderBindingTableRequirements override;

    auto get_queue(QueueType type) -> Ref<Queue> override;

    auto create_command_pool(CommandPoolDesc const& desc) -> Box<CommandPool> override;

    auto create_swapchain(SwapchainDesc const& desc) -> Box<Swapchain> override;

    auto create_fence() -> Box<Fence> override;

    auto create_semaphore() -> Box<Semaphore> override;

    auto create_buffer(BufferDesc const& desc) -> Box<Buffer> override;

    auto create_texture(TextureDesc const& desc) -> Box<Texture> override;

    auto create_sampler(SamplerDesc const& desc) -> Box<Sampler> override;

    auto create_acceleration_structure(AccelerationStructureDesc const& desc) -> Box<AccelerationStructure> override;

    auto create_descriptor_heap(DescriptorHeapDesc const& desc) -> Box<DescriptorHeap> override;

    auto create_shader_module(ShaderModuleDesc const& desc) -> Box<ShaderModule> override;

    auto create_graphics_pipeline(GraphicsPipelineDesc const& desc) -> Box<GraphicsPipeline> override;

    auto create_compute_pipeline(ComputePipelineDesc const& desc) -> Box<ComputePipeline> override;

    auto create_raytracing_pipeline(RaytracingPipelineDesc const& desc) -> Box<RaytracingPipeline> override;

    auto create_descriptor(BufferDescriptorDesc const& buffer_desc, DescriptorHandle handle) -> void override;
    auto create_descriptor(TextureDescriptorDesc const& texture_desc, DescriptorHandle handle) -> void override;
    auto create_descriptor(Ref<Sampler> sampler, DescriptorHandle handle) -> void override;

    auto copy_descriptors(
        DescriptorHandle dst_desciptor,
        CSpan<DescriptorHandle> src_descriptors,
        BindGroupLayout const& bind_group_layout
    ) -> void override;

    auto initialize_pipeline_cache_from(std::string_view cache_file_path) -> void override;

    auto get_acceleration_structure_memory_size(
        AccelerationStructureGeometryBuildInput const& build_info
    ) -> AccelerationStructureMemoryInfo override;
    auto get_acceleration_structure_memory_size(
        AccelerationStructureInstanceBuildInput const& build_info
    ) -> AccelerationStructureMemoryInfo override;

    auto raw() const -> ID3D12Device5* { return device_.Get(); }

    auto raw_factory() const -> IDXGIFactory6* { return factory_.Get(); }

    auto pipeline_cache() -> PipelineStateCacheD3D12& { return pipeline_cahce_; }

    auto allocator() const -> D3D12MA::Allocator* { return allocator_; }

    auto rtv_heap() const -> Ref<RenderTargetDescriptorHeapD3D12> { return rtv_heap_.ref(); }
    auto dsv_heap() const -> Ref<RenderTargetDescriptorHeapD3D12> { return dsv_heap_.ref(); }

    auto get_local_root_signature(uint32_t size_in_bytes, uint32_t space, uint32_t register_) -> ID3D12RootSignature*;

private:
    auto initialize_device_properties() -> void;

    auto create_queues() -> void;

    auto create_cpu_descriptor_heaps() -> void;

    auto save_pso_cache() -> void;

    IDXGIAdapter3* adapter_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Debug3> debug_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device5> device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue_ = nullptr;

    PipelineStateCacheD3D12 pipeline_cahce_;
    std::string pso_cache_file_path_;

    std::array<Box<struct QueueD3D12>, 3> queues_;

    D3D12MA::Allocator* allocator_ = nullptr;

    Box<RenderTargetDescriptorHeapD3D12> rtv_heap_;
    Box<RenderTargetDescriptorHeapD3D12> dsv_heap_;

    std::unordered_map<
        std::tuple<uint32_t, uint32_t, uint32_t>,
        Microsoft::WRL::ComPtr<ID3D12RootSignature>
    > caced_local_root_signatures_;
};

}

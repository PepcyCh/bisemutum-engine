#pragma once

#include "../prelude/ref.hpp"
#include "queue.hpp"
#include "swapchain.hpp"
#include "sync.hpp"
#include "resource.hpp"
#include "sampler.hpp"
#include "shader.hpp"
#include "pipeline.hpp"

namespace bi::rhi {

struct DeviceDesc final {
    Backend backend = Backend::vulkan;
    bool enable_validation = false;
};

struct DeviceProperties final {
    std::string gpu_name;
    uint32_t max_num_bind_groups = ~0u;
    bool separate_sampler_heap : 1 = true;
    bool descriptor_heap_suballocation : 1 = true;
    bool meshlet_pipeline : 1 = false;
    bool raytracing_pipeline : 1 = false;
};

struct Device {
    virtual ~Device() = default;

    static auto create(DeviceDesc const& desc) -> Box<Device>;

    virtual auto get_backend() const -> Backend = 0;

    auto properties() const -> DeviceProperties const& { return device_properties_; }

    virtual auto raytracing_shader_binding_table_requirements() const -> RaytracingShaderBindingTableRequirements = 0;

    virtual auto get_queue(QueueType type) -> Ref<Queue> = 0;

    virtual auto create_command_pool(CommandPoolDesc const& desc) -> Box<CommandPool> = 0;

    virtual auto create_swapchain(SwapchainDesc const& desc) -> Box<Swapchain> = 0;

    virtual auto create_fence() -> Box<Fence> = 0;

    virtual auto create_semaphore() -> Box<Semaphore> = 0;

    virtual auto create_buffer(BufferDesc const& desc) -> Box<Buffer> = 0;

    virtual auto create_texture(TextureDesc const& desc) -> Box<Texture> = 0;

    virtual auto create_sampler(SamplerDesc const& desc) -> Box<Sampler> = 0;

    virtual auto create_acceleration_structure(AccelerationStructureDesc const& desc) -> Box<AccelerationStructure> = 0;

    virtual auto create_descriptor_heap(DescriptorHeapDesc const& desc) -> Box<DescriptorHeap> = 0;

    virtual auto create_shader_module(ShaderModuleDesc const& desc) -> Box<ShaderModule> = 0;

    virtual auto create_graphics_pipeline(GraphicsPipelineDesc const& desc) -> Box<GraphicsPipeline> = 0;

    virtual auto create_compute_pipeline(ComputePipelineDesc const& desc) -> Box<ComputePipeline> = 0;

    virtual auto create_raytracing_pipeline(RaytracingPipelineDesc const& desc) -> Box<RaytracingPipeline> = 0;

    virtual auto create_descriptor(BufferDescriptorDesc const& buffer_desc, DescriptorHandle handle) -> void = 0;
    virtual auto create_descriptor(TextureDescriptorDesc const& texture_desc, DescriptorHandle handle) -> void = 0;
    virtual auto create_descriptor(Ref<Sampler> sampler, DescriptorHandle handle) -> void = 0;
    virtual auto create_descriptor(Ref<AccelerationStructure> accel, DescriptorHandle handle) -> void = 0;

    virtual auto copy_descriptors(
        DescriptorHandle dst_desciptor,
        CSpan<DescriptorHandle> src_descriptors,
        BindGroupLayout const& bind_group_layout
    ) -> void = 0;

    virtual auto initialize_pipeline_cache_from(std::string_view cache_file_path) -> void = 0;

    virtual auto get_acceleration_structure_memory_size(
        AccelerationStructureGeometryBuildInput const& build_info
    ) -> AccelerationStructureMemoryInfo = 0;
    virtual auto get_acceleration_structure_memory_size(
        AccelerationStructureInstanceBuildInput const& build_info
    ) -> AccelerationStructureMemoryInfo = 0;

protected:
    DeviceProperties device_properties_;
};

}

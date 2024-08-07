#pragma once

#include "shader.hpp"
#include "shader_param.hpp"
#include "handles.hpp"
#include "../rhi/command.hpp"

namespace bi::gfx {

struct RenderGraph;
struct Camera;
struct GpuSceneSystem;

struct ResourceBindingContext final {
    ResourceBindingContext();

    auto set_shader_params(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set, BitFlags<rhi::ShaderStage> visibility,
        ShaderParameter& params
    ) -> void;
    auto set_samplers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set) -> void;

    auto set_shader_params(
        Ref<rhi::ComputeCommandEncoder> cmd_encoder, uint32_t set, BitFlags<rhi::ShaderStage> visibility,
        ShaderParameter& params
    ) -> void;
    auto set_samplers(Ref<rhi::ComputeCommandEncoder> cmd_encoder, uint32_t set) -> void;

    auto set_shader_params(
        Ref<rhi::RaytracingCommandEncoder> cmd_encoder, uint32_t set, BitFlags<rhi::ShaderStage> visibility,
        ShaderParameter& params
    ) -> void;
    auto set_samplers(Ref<rhi::RaytracingCommandEncoder> cmd_encoder, uint32_t set) -> void;

    struct SetSamplers final {
        std::vector<rhi::DescriptorHandle> cpu_descriptors;
        rhi::BindGroupLayout layout;
    }; 
    std::vector<SetSamplers> temp_set_samplers;
};

struct GraphicsPassContext final {
    GraphicsPassContext(
        CRef<RenderGraph> rg,
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder,
        std::vector<rhi::ResourceFormat>&& color_targets_format,
        rhi::ResourceFormat depth_stencil_format
    );

    auto render_list(RenderedObjectListHandle list, ShaderParameter& params) const -> void;

    auto render_full_screen(
        CRef<Camera> camera, CRef<FragmentShader> fragment_shader, ShaderParameter& params
    ) const -> void;

    CRef<RenderGraph> rg;
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder;
    std::vector<rhi::ResourceFormat> color_targets_format;
    rhi::ResourceFormat depth_stencil_format;

private:
    Box<ResourceBindingContext> resource_binding_ctx_;
};

struct ComputePassContext final {
    ComputePassContext(
        CRef<RenderGraph> rg,
        Ref<rhi::ComputeCommandEncoder> cmd_encoder
    );

    auto dispatch(
        CRef<ComputeShader> compute_shader, ShaderParameter& params,
        uint32_t num_groups_x = 1, uint32_t num_groups_y = 1, uint32_t num_groups_z = 1
    ) const -> void;

    auto dispatch(
        CRef<Camera> camera, CRef<ComputeShader> compute_shader, ShaderParameter& params,
        uint32_t num_groups_x = 1, uint32_t num_groups_y = 1, uint32_t num_groups_z = 1
    ) const -> void;

    CRef<RenderGraph> rg;
    Ref<rhi::ComputeCommandEncoder> cmd_encoder;

private:
    Box<ResourceBindingContext> resource_binding_ctx_;
};

struct RaytracingPassContext final {
    RaytracingPassContext(
        CRef<RenderGraph> rg,
        Ref<rhi::RaytracingCommandEncoder> cmd_encoder,
        Ref<GpuSceneSystem> gpu_scene
    );

    auto dispatch_rays(
        CRef<Camera> camera, CRef<RaytracingShaders> raytracing_shaders, ShaderParameter& params,
        uint32_t width = 1, uint32_t height = 1, uint32_t depth = 1
    ) const -> void;

    CRef<RenderGraph> rg;
    Ref<rhi::RaytracingCommandEncoder> cmd_encoder;
    Ref<GpuSceneSystem> gpu_scene;

private:
    Box<ResourceBindingContext> resource_binding_ctx_;
};

}

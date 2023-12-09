#pragma once

#include "shader_param.hpp"
#include "handles.hpp"
#include "../rhi/command.hpp"

namespace bi::gfx {

struct RenderGraph;
struct Camera;
struct FragmentShader;

struct ResourceBindingContext final {
    ResourceBindingContext();

    auto set_shader_params(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set, BitFlags<rhi::ShaderStage> visibility,
        ShaderParameter& params
    ) -> void;
    auto set_samplers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set) -> void;

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
        CRef<Camera> camera, CRef<FragmentShader> fragmeng_shader, ShaderParameter& params
    ) const -> void;

    CRef<RenderGraph> rg;
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder;
    std::vector<rhi::ResourceFormat> color_targets_format;
    rhi::ResourceFormat depth_stencil_format;

private:
    Box<ResourceBindingContext> resource_binding_ctx_;
};

struct ComputePassContext final {
    CRef<RenderGraph> rg;
    Ref<rhi::ComputeCommandEncoder> cmd_encoder;
    // TODO - compute pass context
};

}

#pragma once

#include "mipmap_mode.hpp"
#include "renderer_list.hpp"
#include "../rhi/command.hpp"

namespace bi::gfx {

struct ResourceBindingContext final {
    ResourceBindingContext();

    auto set_shader_params(Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set, ShaderParameter& params) -> void;
    auto set_samplers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set) -> void;

    struct SetSamplers final {
        std::vector<rhi::DescriptorHandle> cpu_descriptors;
        rhi::BindGroupLayout layout;
    }; 
    std::vector<SetSamplers> temp_set_samplers;
};

struct GraphicsPassContext final {
    GraphicsPassContext(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder,
        std::vector<rhi::ResourceFormat>&& color_targets_format,
        rhi::ResourceFormat depth_stencil_format
    );

    auto get_rendered_object_list(RenderedObjectListDesc const& desc) const -> RenderedObjectList;

    auto render_list(RenderedObjectList& list, ShaderParameter& params) const -> void;

    Ref<rhi::GraphicsCommandEncoder> cmd_encoder;
    std::vector<rhi::ResourceFormat> color_targets_format;
    rhi::ResourceFormat depth_stencil_format;

private:
    Box<ResourceBindingContext> resource_binding_ctx_;
};

struct ComputePassContext final {
    Ref<rhi::ComputeCommandEncoder> cmd_encoder;
    // TODO - compute pass context
};

}

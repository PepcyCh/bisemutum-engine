#pragma once

#include <bisemutum/graphics/shader_compiler.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/render_graph_context.hpp>
#include <bisemutum/graphics/mipmap_mode.hpp>
#include <bisemutum/rhi/command.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/prelude/hash.hpp>

namespace bi::gfx {

struct CommandHelpers final {
    auto initialize(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void;

    auto blit_2d(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
        Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void;

    auto generate_mipmaps_2d(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> texture,
        BitFlags<rhi::ResourceAccessType>& texture_access,
        MipmapMode mode
    ) -> void;

    auto equitangular_to_cubemap(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
        Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void;

private:
    auto initialize_blit(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void;
    auto get_blit_pipeline(Ref<Texture> dst_texture) -> Ref<rhi::GraphicsPipeline>;

    auto initialize_mipmap(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void;
    auto get_mipmap_pipeline(Ref<Texture> dst_texture, MipmapMode mode) -> Ref<rhi::GraphicsPipeline>;

    auto initialize_equitangular_to_cubemap(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void;

    Ptr<rhi::Device> device_;

    Box<rhi::Sampler> blit_sampler_;
    Ptr<rhi::ShaderModule> blit_vs_;
    Ptr<rhi::ShaderModule> blit_fs_;
    Ptr<rhi::ShaderModule> blit_fs_depth_;
    std::unordered_map<rhi::ResourceFormat, Box<rhi::GraphicsPipeline>> blit_pipelines_;

    Ptr<rhi::ShaderModule> mipmap_vs_;
    Ptr<rhi::ShaderModule> mipmap_fs_[3];
    Ptr<rhi::ShaderModule> mipmap_fs_depth_[3];
    Box<rhi::ComputePipeline> mipmap_pipelines_compute_[3];
    std::unordered_map<std::pair<rhi::ResourceFormat, MipmapMode>, Box<rhi::GraphicsPipeline>> mipmap_pipelines_;

    Box<rhi::ComputePipeline> equitangular_to_cubemap_pipeline_;
};

}

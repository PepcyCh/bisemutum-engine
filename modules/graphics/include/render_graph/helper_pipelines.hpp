#pragma once

#include "graphics/device.hpp"
#include "graphics/command.hpp"
#include "shader_manager/shader_manager.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class MipmapMode : uint8_t {
    eAverage,
    eMin,
    eMax,
};

class HelperPipelines {
public:
    HelperPipelines(Ref<gfx::Device> device);

    void BlitTexture(Ref<gfx::CommandEncoder> cmd_encoder, const gfx::TextureView &src_view,
        const gfx::TextureView &dst_view) const;

    void GenerateMipmaps2D(Ref<gfx::CommandEncoder> cmd_encoder, Ref<gfx::Texture> texture,
        gfx::ResourceAccessType &tex_access_type, MipmapMode mode = MipmapMode::eAverage) const;

private:
    void InitBlitPipelines();
    void InitMipmapPipelines();

    Ref<gfx::Device> device_;
    Ptr<ShaderManager> shader_manager_;

    Ptr<gfx::RenderPipeline> blit_pipeline_;
    Ptr<gfx::RenderPipeline> blit_pipeline_depth_;
    Ptr<gfx::Sampler> blit_sampler_;

    Ptr<gfx::RenderPipeline> mipmap_pipeline_avg_render_;
    Ptr<gfx::RenderPipeline> mipmap_pipeline_min_render_;
    Ptr<gfx::RenderPipeline> mipmap_pipeline_max_render_;
    Ptr<gfx::ComputePipeline> mipmap_pipeline_avg_compute_;
    Ptr<gfx::ComputePipeline> mipmap_pipeline_min_compute_;
    Ptr<gfx::ComputePipeline> mipmap_pipeline_max_compute_;
    Ptr<gfx::RenderPipeline> mipmap_pipeline_avg_depth_;
    Ptr<gfx::RenderPipeline> mipmap_pipeline_min_depth_;
    Ptr<gfx::RenderPipeline> mipmap_pipeline_max_depth_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

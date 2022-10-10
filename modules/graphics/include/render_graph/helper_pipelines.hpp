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
    HelperPipelines(Ref<Device> device);

    void BlitTexture(Ref<CommandEncoder> cmd_encoder, const TextureView &src_view,
        const TextureView &dst_view) const;

    void GenerateMipmaps2D(Ref<CommandEncoder> cmd_encoder, Ref<Texture> texture,
        ResourceAccessType &tex_access_type, MipmapMode mode = MipmapMode::eAverage) const;

private:
    void InitBlitPipelines();
    void InitMipmapPipelines();

    Ref<Device> device_;
    Ptr<ShaderManager> shader_manager_;

    Ptr<RenderPipeline> blit_pipeline_;
    Ptr<RenderPipeline> blit_pipeline_depth_;
    Ptr<Sampler> blit_sampler_;

    Ptr<RenderPipeline> mipmap_pipeline_avg_render_;
    Ptr<RenderPipeline> mipmap_pipeline_min_render_;
    Ptr<RenderPipeline> mipmap_pipeline_max_render_;
    Ptr<ComputePipeline> mipmap_pipeline_avg_compute_;
    Ptr<ComputePipeline> mipmap_pipeline_min_compute_;
    Ptr<ComputePipeline> mipmap_pipeline_max_compute_;
    Ptr<RenderPipeline> mipmap_pipeline_avg_depth_;
    Ptr<RenderPipeline> mipmap_pipeline_min_depth_;
    Ptr<RenderPipeline> mipmap_pipeline_max_depth_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

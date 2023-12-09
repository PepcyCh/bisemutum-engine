#include <bisemutum/graphics/camera.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/runtime/frame_timer.hpp>

namespace bi::gfx {

namespace {

BI_SHADER_PARAMETERS_BEGIN(FrameInfo)
    BI_SHADER_PARAMETER(uint, index)
    BI_SHADER_PARAMETER(float, time_seconds)
    BI_SHADER_PARAMETER(float2, _pad)
BI_SHADER_PARAMETERS_END(FrameInfo)

BI_SHADER_PARAMETERS_BEGIN(Viewport)
    BI_SHADER_PARAMETER(uint2, size)
    BI_SHADER_PARAMETER(uint2, offset)
BI_SHADER_PARAMETERS_END(Viewport)

BI_SHADER_PARAMETERS_BEGIN(CameraData)
    BI_SHADER_PARAMETER(float4x4, matrix_view)
    BI_SHADER_PARAMETER(float4x4, matrix_inv_view)
    BI_SHADER_PARAMETER(float4x4, matrix_proj)
    BI_SHADER_PARAMETER(float4x4, matrix_inv_proj)
    BI_SHADER_PARAMETER(float4x4, matrix_proj_view)
    BI_SHADER_PARAMETER(float4x4, matrix_prev_proj_view)
BI_SHADER_PARAMETERS_END(CameraData)

BI_SHADER_PARAMETERS_BEGIN(GraphicsInput)
    BI_SHADER_PARAMETER(FrameInfo, frame);
    BI_SHADER_PARAMETER(Viewport, viewport);
    BI_SHADER_PARAMETER_INCLUDE(CameraData, camera);
BI_SHADER_PARAMETERS_END(GraphicsInput)

} // namespace

auto Camera::recreate_target_texture(uint32_t width, uint32_t height, rhi::ResourceFormat format, bool mipmap) -> void {
    auto usages = BitFlags{
        rhi::is_color_format(format)
            ? rhi::TextureUsage::color_attachment : rhi::TextureUsage::depth_stencil_attachment,
        rhi::TextureUsage::sampled
    };
    recreate_target_texture(rhi::TextureDesc{
        .extent = {width, height, 1},
        .levels = mipmap ? static_cast<uint32_t>(1 + std::log2(std::max(width, height))) : 1,
        .format = format,
        .dim = rhi::TextureDimension::d2,
        .usages = usages,
    });
}

auto Camera::recreate_target_texture(rhi::TextureDesc const& desc) -> void {
    if (!target_texture_.has_value() || desc != target_texture_.desc()) {
        target_texture_ = Texture(desc);
        target_texture_state_preinitialized_ = true;
    }
}

auto Camera::shader_params() -> ShaderParameter& {
    return shader_parameter_;
}
auto Camera::shader_params_metadata() const -> ShaderParameterMetadataList const& {
    return shader_parameter_.metadata_list();
}

auto Camera::update_shader_params() -> void {
    if (!shader_parameter_) {
        shader_parameter_.initialize<GraphicsInput>();
    }

    auto uniform_data = shader_parameter_.mutable_typed_data<GraphicsInput>();

    uniform_data->frame.index = g_engine->window()->frame_count();
    uniform_data->frame.time_seconds = g_engine->frame_timer()->total_time();

    auto aspect = 1.0f;
    if (target_texture_.has_value()) {
        aspect = static_cast<float>(target_texture_.desc().extent.width)
            / target_texture_.desc().extent.height;
        uniform_data->viewport.size = {
            target_texture_.desc().extent.width,
            target_texture_.desc().extent.height,
        };
    } else {
        uniform_data->viewport.size = {0u, 0u};
    }
    uniform_data->viewport.offset = {0u, 0u};

    uniform_data->camera.matrix_view = math::lookAt(position, position + front_dir, up_dir);
    if (projection_type == ProjectionType::perspective) {
        uniform_data->camera.matrix_proj = math::perspective(math::radians(yfov), aspect, 0.001f, 1e5f);
    } else {
        auto ortho_height = std::tan(math::radians(yfov * 0.5f));
        auto ortho_width = ortho_height * aspect;
        uniform_data->camera.matrix_proj = math::ortho(
            -ortho_width, ortho_width, -ortho_height, ortho_height, near_z, far_z
        );
    }
    uniform_data->camera.matrix_inv_view = math::inverse(uniform_data->camera.matrix_view);
    uniform_data->camera.matrix_inv_proj = math::inverse(uniform_data->camera.matrix_proj);
    uniform_data->camera.matrix_proj_view = uniform_data->camera.matrix_proj * uniform_data->camera.matrix_view;
    uniform_data->camera.matrix_prev_proj_view = matrix_proj_view_;
    matrix_proj_view_ = uniform_data->camera.matrix_proj_view;

    shader_parameter_.update_uniform_buffer();
}

}

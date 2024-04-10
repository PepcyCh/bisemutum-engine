#include <bisemutum/graphics/camera.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/runtime/frame_timer.hpp>

namespace bi::gfx {

namespace {

BI_SHADER_PARAMETERS_BEGIN(FrameInfo)
    BI_SHADER_PARAMETER(uint, index)
    BI_SHADER_PARAMETER(float, time_seconds)
    BI_SHADER_PARAMETER(float2, _pad)
BI_SHADER_PARAMETERS_END()

BI_SHADER_PARAMETERS_BEGIN(Viewport)
    BI_SHADER_PARAMETER(uint2, size)
    BI_SHADER_PARAMETER(uint2, offset)
BI_SHADER_PARAMETERS_END()

BI_SHADER_PARAMETERS_BEGIN(CameraData)
    BI_SHADER_PARAMETER(float4x4, matrix_view)
    BI_SHADER_PARAMETER(float4x4, matrix_inv_view)
    BI_SHADER_PARAMETER(float4x4, matrix_proj)
    BI_SHADER_PARAMETER(float4x4, matrix_inv_proj)
    BI_SHADER_PARAMETER(float4x4, matrix_proj_view)
    BI_SHADER_PARAMETER(float4x4, matrix_prev_proj_view)
    BI_SHADER_PARAMETER(float4x4, history_matrix_proj_view)
    BI_SHADER_PARAMETER(float4x4, history_matrix_inv_proj)
    BI_SHADER_PARAMETER(float4x4, history_matrix_inv_view)
BI_SHADER_PARAMETERS_END()

BI_SHADER_PARAMETERS_BEGIN(GraphicsInput)
    BI_SHADER_PARAMETER(FrameInfo, frame);
    BI_SHADER_PARAMETER(Viewport, viewport);
    BI_SHADER_PARAMETER_INCLUDE(CameraData, camera);
BI_SHADER_PARAMETERS_END()

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
        uniform_data->camera.matrix_proj = math::perspective_reverse_z(math::radians(yfov), aspect, near_z, far_z);
    } else {
        auto ortho_height = std::tan(math::radians(yfov * 0.5f));
        auto ortho_width = ortho_height * aspect;
        uniform_data->camera.matrix_proj = math::ortho_reverse_z(
            -ortho_width, ortho_width, -ortho_height, ortho_height, near_z, far_z
        );
    }
    uniform_data->camera.matrix_inv_view = math::inverse(uniform_data->camera.matrix_view);
    uniform_data->camera.matrix_inv_proj = math::inverse(uniform_data->camera.matrix_proj);
    uniform_data->camera.matrix_proj_view = uniform_data->camera.matrix_proj * uniform_data->camera.matrix_view;
    uniform_data->camera.matrix_prev_proj_view = matrix_proj_view_;
    matrix_proj_view_ = uniform_data->camera.matrix_proj_view;

    shader_parameter_.update_uniform_buffer();

    uniform_data->camera.history_matrix_proj_view = uniform_data->camera.matrix_proj_view;
    uniform_data->camera.history_matrix_inv_proj = uniform_data->camera.matrix_inv_proj;
    uniform_data->camera.history_matrix_inv_view = uniform_data->camera.matrix_inv_view;
}

auto Camera::get_frustum_planes() const -> std::array<float4, 6> {
    std::array<float4, 6> planes;

    auto front = math::normalize(front_dir);
    auto right = math::normalize(math::cross(front, up_dir));
    auto up = math::cross(right, front);

    auto aspect = 1.0f;
    if (target_texture_.has_value()) {
        aspect = static_cast<float>(target_texture_.desc().extent.width)
            / target_texture_.desc().extent.height;
    }
    auto xfov = yfov * aspect;

    auto pos_dot_front = math::dot(position, front);
    planes[0] = float4(front, -near_z - pos_dot_front);
    planes[1] = float4(-front, pos_dot_front + far_z);

    if (projection_type == ProjectionType::perspective) {
        auto vert_angle = math::radians(90.0f - yfov * 0.5f);
        auto vert_rot_mat_top = math::rotate(float4x4{1.0f}, -vert_angle, right);
        planes[2] = float4(vert_rot_mat_top * float4(front, 0.0f));
        planes[2].w = -math::dot(position, float3(planes[2]));
        auto vert_rot_mat_down = math::rotate(float4x4{1.0f}, vert_angle, right);
        planes[3] = float4(vert_rot_mat_down * float4(front, 0.0f));
        planes[3].w = -math::dot(position, float3(planes[3]));

        auto hori_angle = math::radians(90.0f - xfov * 0.5f);
        auto hori_rot_mat_left = math::rotate(float4x4{1.0f}, -hori_angle, up);
        planes[4] = float4(hori_rot_mat_left * float4(front, 0.0f));
        planes[4].w = -math::dot(position, float3(planes[4]));
        auto hori_rot_mat_right = math::rotate(float4x4{1.0f}, hori_angle, up);
        planes[5] = float4(hori_rot_mat_right * float4(front, 0.0f));
        planes[5].w = -math::dot(position, float3(planes[5]));
    } else {
        auto ortho_height = std::tan(math::radians(yfov * 0.5f));
        auto ortho_width = ortho_height * aspect;

        auto pos_dot_up = math::dot(position, up);
        planes[2] = float4(-up, ortho_height + pos_dot_up);
        planes[3] = float4(up, ortho_height - pos_dot_up);

        auto pos_dot_right = math::dot(position, right);
        planes[4] = float4(right, ortho_width - pos_dot_up);
        planes[5] = float4(-right, ortho_width + pos_dot_up);
    }

    return planes;
}

auto Camera::add_history_buffer(std::string key, BufferHandle handle) const -> void {
    auto& rg = g_engine->graphics_manager()->render_graph();
    history_buffers_[history_index_].insert({std::move(key), rg.take_buffer(handle)});
}
auto Camera::add_history_texture(std::string key, TextureHandle handle) const -> void {
    auto& rg = g_engine->graphics_manager()->render_graph();
    history_textures_[history_index_].insert({std::move(key), rg.take_texture(handle)});
}
auto Camera::get_history_buffer(std::string_view key) const -> BufferHandle {
    auto& rg = g_engine->graphics_manager()->render_graph();
    if (auto it = history_buffers_[history_index_ ^ 1].find(key); it != history_buffers_[history_index_ ^ 1].end()) {
        return rg.import_buffer(it->second.ref());
    } else {
        return BufferHandle::invalid;
    }
}
auto Camera::get_history_texture(std::string_view key) const -> TextureHandle {
    auto& rg = g_engine->graphics_manager()->render_graph();
    if (auto it = history_textures_[history_index_ ^ 1].find(key); it != history_textures_[history_index_ ^ 1].end()) {
        return rg.import_texture(it->second.ref());
    } else {
        return TextureHandle::invalid;
    }
}
auto Camera::clear_history_resources() -> void {
    history_index_ ^= 1;
    history_buffers_[history_index_].clear();
    history_textures_[history_index_].clear();
}

}

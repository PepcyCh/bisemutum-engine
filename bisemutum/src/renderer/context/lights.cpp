#include "lights.hpp"

#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/scene_basic/light.hpp>

namespace bi {

LightsContext::LightsContext() {
    shadow_map_sampler = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_border,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_border,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_border,
        .border_color = rhi::SamplerBorderColor::white_float,
    });
}

auto LightsContext::collect_all_lights() -> void {
    dir_lights.clear();
    point_lights.clear();
    spot_lights.clear();

    dir_lights_with_shadow.clear();
    dir_lights_shadow_transform.clear();

    auto scene = g_engine->world()->current_scene().value();

    auto lights_view = scene->ecs_registry().view<LightComponent>();
    for (auto entity : lights_view) {
        auto object = scene->object_of(entity);
        auto& transform = object->world_transform();
        auto& light = lights_view.get<LightComponent>(entity);
        LightData data{};
        data.emission = light.color * light.strength;
        switch (light.type) {
            case LightType::directional:
                data.direction = transform.transform_direction({0.0f, 1.0f, 0.0f});
                if (light.cast_shadow && light.shadow_strength > 0.0f) {
                    data.sm_index = dir_lights_with_shadow.size();
                    data.shadow_strength = std::min(light.shadow_strength, 1.0f);
                    auto& shadow_dir_light = dir_lights_with_shadow.emplace_back();
                    shadow_dir_light.camera.position = 20.0f * data.direction;
                    shadow_dir_light.camera.front_dir = -data.direction;
                    shadow_dir_light.camera.up_dir = float3(0.0f, 1.0f, 0.0f);
                    if (std::abs(shadow_dir_light.camera.front_dir.y) > 0.99f) {
                        shadow_dir_light.camera.up_dir = float3(1.0f, 0.0f, 0.0f);
                    }
                    shadow_dir_light.camera.yfov = 2.0f * math::degrees(std::atan(20.0f));
                    shadow_dir_light.camera.near_z = 0.0001f;
                    shadow_dir_light.camera.far_z = 100.0f;
                    shadow_dir_light.camera.projection_type = gfx::ProjectionType::orthographic;
                    shadow_dir_light.camera.update_shader_params();
                    dir_lights_shadow_transform.push_back(shadow_dir_light.camera.matrix_proj_view());
                }
                dir_lights.push_back(data);
                break;
            case LightType::point:
                data.position = transform.transform_position({0.0f, 0.0f, 0.0f});
                data.range_sqr = light.range * light.range;
                point_lights.push_back(data);
                break;
            case LightType::spot:
                data.position = transform.transform_position({0.0f, 0.0f, 0.0f});
                data.direction = transform.transform_direction({0.0f, 1.0f, 0.0f});
                data.cos_outer = std::cos(math::radians(light.spot_outer_angle));
                data.cos_inner = std::cos(math::radians(light.spot_inner_angle));
                data.range_sqr = light.range * light.range;
                spot_lights.push_back(data);
                break;
            default: break;
        }
    }

    auto update_buffer = [](gfx::Buffer& buffer, auto& lights) {
        auto need_buffer_size = std::max<size_t>(lights.size(), 1) * sizeof(lights[0]);
        if (!buffer.has_value() || need_buffer_size < buffer.desc().size) {
            buffer = gfx::Buffer(
                gfx::BufferBuilder()
                    .size(need_buffer_size)
                    .usage(rhi::BufferUsage::storage_read)
            );
        }
        if (!lights.empty()) {
            buffer.set_data(lights.data(), lights.size());
        }
    };
    update_buffer(dir_lights_buffer, dir_lights);
    update_buffer(point_lights_buffer, point_lights);
    update_buffer(spot_lights_buffer, spot_lights);
    update_buffer(dir_lights_shadow_transform_buffer, dir_lights_shadow_transform);

    auto dir_lights_shadow_map_layers = std::max<uint32_t>(1, dir_lights_with_shadow.size());
    if (
        !dir_lights_shadow_map.has_value()
        || dir_lights_shadow_map.desc().extent.depth_or_layers < dir_lights_shadow_map_layers
        || dir_lights_shadow_map.desc().extent.depth_or_layers > 2 * dir_lights_shadow_map_layers
    ) {
        dir_lights_shadow_map = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_2d(rhi::ResourceFormat::d32_sfloat, 1024, 1024, dir_lights_shadow_map_layers)
                .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled})
        );
    }
}

}

#include "lights.hpp"

#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/graphics/utils.hpp>
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
    auto scene = g_engine->world()->current_scene().value();

    auto dir_lights_view = scene->ecs_registry().view<DirectionalLightComponent>();
    dir_lights.clear();
    dir_lights_with_shadow.clear();
    for (auto entity : dir_lights_view) {
        auto& light = dir_lights_view.get<DirectionalLightComponent>(entity);
        DirLightData data{};
        data.emission = light.color * light.strength;
        if (data.emission == float3(0.0f)) {
            continue;
        }
        auto object = scene->object_of(entity);
        auto& transform = object->world_transform();
        data.direction = transform.transform_direction({0.0f, 1.0f, 0.0f});
        if (light.cast_shadow && light.shadow_strength > 0.0f && light.shadow_range > 0.0f) {
            light.cascade_shadow_ratio.z = std::min(light.cascade_shadow_ratio.z, 1.0f);
            light.cascade_shadow_ratio.y = std::min(light.cascade_shadow_ratio.y, light.cascade_shadow_ratio.z);
            light.cascade_shadow_ratio.x = std::min(light.cascade_shadow_ratio.x, light.cascade_shadow_ratio.y);
            data.cascade_shadow_radius_sqr = float4{light.cascade_shadow_ratio, 1.0f} * light.shadow_range;
            data.shadow_depth_bias = light.shadow_depth_bias_factor;
            data.shadow_normal_bias = 0.5f * data.cascade_shadow_radius_sqr.w / dir_light_shadow_map_resolution
                * light.shadow_normal_bias_factor;

            auto up_dir = float3{0.0f, 1.0f, 0.0f};
            if (std::abs(data.direction.y) > 0.99f) {
                up_dir = float3{1.0f, 0.0f, 0.0f};
            }

            data.sm_index = dir_lights_with_shadow.size();
            data.shadow_strength = std::min(light.shadow_strength, 1.0f);
            for (int level = 0; level < 4; level++) {
                auto& shadow_dir_light = dir_lights_with_shadow.emplace_back();
                shadow_dir_light.camera.front_dir = -data.direction;
                shadow_dir_light.camera.up_dir = up_dir;
                auto size = data.cascade_shadow_radius_sqr[level];
                shadow_dir_light.shadow_size = size;
                shadow_dir_light.camera.yfov = 2.0f * math::degrees(std::atan(size));
                shadow_dir_light.camera.near_z = 0.0001f;
                shadow_dir_light.camera.far_z = size * 2.01f;
                shadow_dir_light.camera.projection_type = gfx::ProjectionType::orthographic;
            }

            data.cascade_shadow_radius_sqr *= data.cascade_shadow_radius_sqr;
        }
        dir_lights.push_back(data);
    }
    gfx::Buffer::update_with_container<true>(dir_lights_buffer, dir_lights);

    dir_lights_shadow_transform.resize(dir_lights_with_shadow.size());
    auto dir_lights_shadow_map_layers = std::max<uint32_t>(1, dir_lights_with_shadow.size());
    if (
        !dir_lights_shadow_map.has_value()
        || dir_lights_shadow_map.desc().extent.width != dir_light_shadow_map_resolution
        || dir_lights_shadow_map.desc().extent.depth_or_layers < dir_lights_shadow_map_layers
        || dir_lights_shadow_map.desc().extent.depth_or_layers > 2 * dir_lights_shadow_map_layers
    ) {
        dir_lights_shadow_map = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_2d(
                    rhi::ResourceFormat::d32_sfloat,
                    dir_light_shadow_map_resolution, dir_light_shadow_map_resolution, dir_lights_shadow_map_layers
                )
                .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled})
        );
    }

    auto point_lights_view = scene->ecs_registry().view<PointLightComponent>();
    point_lights.clear();
    point_lights_with_shadow.clear();
    point_lights_shadow_transform.clear();
    for (auto entity : point_lights_view) {
        auto& light = point_lights_view.get<PointLightComponent>(entity);
        PointLightData data{};
        data.emission = light.color * light.strength;
        if (data.emission == float3(0.0f)) {
            continue;
        }
        auto object = scene->object_of(entity);
        auto& transform = object->world_transform();
        data.position = transform.transform_position({0.0f, 0.0f, 0.0f});
        data.direction = transform.transform_direction({0.0f, 1.0f, 0.0f});
        if (light.spot) {
            data.cos_outer = std::cos(math::radians(light.spot_outer_angle));
            data.cos_inner = std::cos(math::radians(light.spot_inner_angle));
        } else {
            data.cos_outer = 0.0f;
            data.cos_inner = 0.0f;
        }
        data.range_sqr_inv = 1.0f / (light.range * light.range);
        if (light.cast_shadow && light.shadow_strength > 0.0f) {
            data.sm_index = point_lights_with_shadow.size();
            data.shadow_strength = std::min(light.shadow_strength, 1.0f);
            data.shadow_depth_bias = light.shadow_depth_bias_factor * 0.1f;
            auto cam_half_yfov = light.spot ? light.spot_outer_angle : 45.0f;
            data.shadow_normal_bias = std::tan(math::radians(cam_half_yfov)) / point_light_shadow_map_resolution
                * light.shadow_normal_bias_factor * 10.0f;
            if (light.spot) {
                auto& shadow_light = point_lights_with_shadow.emplace_back();
                auto& cam = shadow_light.camera;
                cam.position = data.position;
                cam.front_dir = -data.direction;
                auto up_dir = float3{0.0f, 1.0f, 0.0f};
                if (std::abs(data.direction.y) > 0.99f) {
                    up_dir = float3{1.0f, 0.0f, 0.0f};
                }
                cam.up_dir = up_dir;
                cam.yfov = 2.0f * light.spot_outer_angle;
                cam.near_z = 0.001f;
                cam.far_z = light.shadow_range;
                cam.projection_type = gfx::ProjectionType::perspective;
                cam.update_shader_params();
                point_lights_shadow_transform.push_back(cam.matrix_proj_view());
            } else {
                for (size_t face = 0; face < 6; face++) {
                    auto& shadow_light = point_lights_with_shadow.emplace_back();
                    auto& cam = shadow_light.camera;
                    cam.position = data.position;
                    cam.front_dir = gfx::cubemap_front_directions[face];
                    cam.up_dir = gfx::cubemap_up_directions[face];
                    cam.yfov = 90.0f + 5.0f;
                    cam.near_z = 0.001f;
                    cam.far_z = light.shadow_range;
                    cam.projection_type = gfx::ProjectionType::perspective;
                    cam.update_shader_params();
                    point_lights_shadow_transform.push_back(cam.matrix_proj_view());
                }
            }
        }
        point_lights.push_back(data);
    }
    gfx::Buffer::update_with_container<true>(point_lights_buffer, point_lights);
    gfx::Buffer::update_with_container<true>(point_lights_shadow_transform_buffer, point_lights_shadow_transform);

    auto point_lights_shadow_map_layers = std::max<uint32_t>(1, point_lights_with_shadow.size());
    if (
        !point_lights_shadow_map.has_value()
        || point_lights_shadow_map.desc().extent.width != point_light_shadow_map_resolution
        || point_lights_shadow_map.desc().extent.depth_or_layers < point_lights_shadow_map_layers
        || point_lights_shadow_map.desc().extent.depth_or_layers > 2 * point_lights_shadow_map_layers
    ) {
        point_lights_shadow_map = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_2d(
                    rhi::ResourceFormat::d32_sfloat,
                    point_light_shadow_map_resolution, point_light_shadow_map_resolution, point_lights_shadow_map_layers
                )
                .usage({rhi::TextureUsage::depth_stencil_attachment, rhi::TextureUsage::sampled})
        );
    }
}

auto LightsContext::prepare_dir_lights_per_camera(gfx::Camera const& camera) -> void {
    for (size_t i = 0; auto& l : dir_lights_with_shadow) {
        l.camera.position = camera.position - l.shadow_size * l.camera.front_dir;
        l.camera.update_shader_params();
        dir_lights_shadow_transform[i] = l.camera.matrix_proj_view();
        ++i;
    }
    gfx::Buffer::update_with_container<true>(dir_lights_shadow_transform_buffer, dir_lights_shadow_transform);
}

}

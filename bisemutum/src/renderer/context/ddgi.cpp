#include "ddgi.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>

namespace bi {

namespace {

constexpr uint32_t ddgi_probes_size = 8;
constexpr uint32_t ddgi_probe_irradiance_size = 8;
constexpr uint32_t ddgi_probe_visibility_size = 16;

constexpr uint32_t ddgi_irradiance_texture_width = ddgi_probes_size * ddgi_probes_size * ddgi_probe_irradiance_size;
constexpr uint32_t ddgi_irradiance_texture_height = ddgi_probes_size * ddgi_probe_irradiance_size;

constexpr uint32_t ddgi_visibility_texture_width = ddgi_probes_size * ddgi_probes_size * ddgi_probe_visibility_size;
constexpr uint32_t ddgi_visibility_texture_height = ddgi_probes_size * ddgi_probe_visibility_size;

}

auto DdgiContext::update_frame() -> void {
    auto scene = g_engine->world()->current_scene().value();

    std::unordered_set<rt::SceneObject::Id> objects_to_be_removed;
    objects_to_be_removed.reserve(ddgi_volumes_data_.size());
    for (auto& [id, _] : ddgi_volumes_data_) {
        objects_to_be_removed.insert(id);
    }

    auto ddgi_volumes_view = scene->ecs_registry().view<DdgiVolumeComponent>();
    for (auto entity : ddgi_volumes_view) {
        auto& ddgi_volume = ddgi_volumes_view.get<DdgiVolumeComponent>(entity);
        auto object = scene->object_of(entity);
        auto id = object->get_id();
        objects_to_be_removed.erase(id);

        auto [ddgi_data_it, is_new_volume] = ddgi_volumes_data_.try_emplace(id);
        if (is_new_volume) {
            ddgi_data_it->second.irradiance = gfx::Texture{
                gfx::TextureBuilder{}
                    .dim_2d(
                        rhi::ResourceFormat::rgba16_sfloat,
                        ddgi_irradiance_texture_width,
                        ddgi_irradiance_texture_height
                    )
                    .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
            };
            ddgi_data_it->second.visibility = gfx::Texture{
                gfx::TextureBuilder{}
                    .dim_2d(
                        rhi::ResourceFormat::rg16_sfloat,
                        ddgi_visibility_texture_width,
                        ddgi_visibility_texture_height
                    )
                    .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
            };
        }

        auto& transform = object->world_transform();
        ddgi_data_it->second.base_point = transform.transform_position({-1.0f, -1.0f, -1.0f});
        ddgi_data_it->second.frame_x = transform.transform_direction_without_scaling({1.0f, 0.0f, 0.0f});
        ddgi_data_it->second.frame_y = transform.transform_direction_without_scaling({0.0f, 1.0f, 0.0f});
        ddgi_data_it->second.frame_z = transform.transform_direction_without_scaling({0.0f, 0.0f, 1.0f});
        ddgi_data_it->second.voluem_extent = transform.scaling;
    }

    for (auto id : objects_to_be_removed) {
        ddgi_volumes_data_.erase(id);
    }
}

}

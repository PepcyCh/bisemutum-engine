#include "ddgi.hpp"

#include <numbers>

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

constexpr uint32_t ddgi_sample_randoms_size = 8192;

}

auto DdgiContext::update_frame() -> void {
    init_sample_randoms();

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

auto DdgiContext::num_ddgi_volumes() const -> uint32_t {
    return ddgi_volumes_data_.size();
}

auto DdgiContext::init_sample_randoms() -> void {
    if (sample_randoms_buffer_.has_value()) { return; }

    constexpr auto phi2 = 1.0 / 1.3247179572447;
    constexpr auto delta = float2{phi2, phi2 * phi2};

    std::vector<float2> sample_randoms(ddgi_sample_randoms_size);
    sample_randoms[0] = {0.5f, 0.5f};
    for (uint32_t i = 1; i < ddgi_sample_randoms_size; i++) {
        sample_randoms[i] = sample_randoms[i - 1] + delta;
        if (sample_randoms[i].x >= 1.0f) { sample_randoms[i].x -= 1.0f; }
        if (sample_randoms[i].y >= 1.0f) { sample_randoms[i].y -= 1.0f; }
    }

    sample_randoms_buffer_ = gfx::Buffer{
        gfx::BufferBuilder{}
            .size(ddgi_sample_randoms_size * sizeof(float2))
            .usage({rhi::BufferUsage::storage_read})
    };
    sample_randoms_buffer_.set_data(sample_randoms.data(), sample_randoms.size());
}

}

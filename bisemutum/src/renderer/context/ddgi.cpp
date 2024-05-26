#include "ddgi.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

DdgiContext::DdgiContext() {
    sampler_ = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
    });

    shader_data_[0].num_volumes = 0;
    shader_data_[0].sampler = {sampler_};

    shader_data_[1].num_volumes = 0;
    shader_data_[1].sampler = {sampler_};
}

auto DdgiContext::update_frame(BasicRenderer::IndirectDiffuseSettings const& settings) -> void {
    init_sample_randoms();

    auto this_frame = g_engine->window()->frame_count();
    auto history_valid = last_frame_ + 1 == this_frame;
    shader_data_[db_ind_].strength = settings.strength;
    if (!history_valid) {
        shader_data_[db_ind_].num_volumes = 0;
        shader_data_[db_ind_].irradiance_texture = {};
        shader_data_[db_ind_].visibility_texture = {};
        shader_data_[db_ind_].volumes = {};
        irradiance_texture_[db_ind_].reset();
        visibility_texture_[db_ind_].reset();
        volumes_buffer_[db_ind_].reset();
    }
    last_frame_ = this_frame;
    db_ind_ ^= 1;

    auto scene = g_engine->world()->current_scene().value();

    std::unordered_set<rt::SceneObject::Id> objects_to_be_removed;
    objects_to_be_removed.reserve(ddgi_volumes_data_.size());
    for (auto& [id, _] : ddgi_volumes_data_) {
        objects_to_be_removed.insert(id);
    }

    auto ddgi_volumes_view = scene->ecs_registry().view<DdgiVolumeComponent>();
    uint32_t index = 0;
    std::vector<DdgiVolumeData> volumes_data;
    for (auto entity : ddgi_volumes_view) {
        auto& ddgi_volume = ddgi_volumes_view.get<DdgiVolumeComponent>(entity);
        auto object = scene->object_of(entity);
        auto id = object->get_id();
        objects_to_be_removed.erase(id);

        auto [ddgi_data_it, _] = ddgi_volumes_data_.try_emplace(id);

        auto& transform = object->world_transform();
        ddgi_data_it->second.prev_index = history_valid ? ddgi_data_it->second.index : 0xffffffffu;
        ddgi_data_it->second.index = index++;
        auto base_position = transform.transform_position({-1.0f, -1.0f, -1.0f});
        auto frame_x = transform.transform_direction_without_scaling({1.0f, 0.0f, 0.0f});
        auto frame_y = transform.transform_direction_without_scaling({0.0f, 1.0f, 0.0f});
        auto frame_z = transform.transform_direction_without_scaling({0.0f, 0.0f, 1.0f});
        if (
            ddgi_data_it->second.base_position != base_position
            || ddgi_data_it->second.frame_x != frame_x
            || ddgi_data_it->second.frame_y != frame_y
            || ddgi_data_it->second.frame_z != frame_z
        ) {
            ddgi_data_it->second.prev_index = 0xffffffffu;
        }
        ddgi_data_it->second.base_position = base_position;
        ddgi_data_it->second.frame_x = frame_x;
        ddgi_data_it->second.frame_y = frame_y;
        ddgi_data_it->second.frame_z = frame_z;
        ddgi_data_it->second.voluem_extent = transform.scaling * 2.0f;

        volumes_data.push_back(DdgiVolumeData{
            .base_position = ddgi_data_it->second.base_position,
            .frame_x = ddgi_data_it->second.frame_x,
            .extent_x = ddgi_data_it->second.voluem_extent.x,
            .frame_y = ddgi_data_it->second.frame_y,
            .extent_y = ddgi_data_it->second.voluem_extent.y,
            .frame_z = ddgi_data_it->second.frame_z,
            .extent_z = ddgi_data_it->second.voluem_extent.z,
        });
    }

    gfx::Buffer::update_with_container<true>(volumes_buffer_[db_ind_], volumes_data);
    shader_data_[db_ind_].strength = settings.strength;
    shader_data_[db_ind_].num_volumes = volumes_data.size();
    shader_data_[db_ind_].volumes = {&volumes_buffer_[db_ind_]};

    for (auto id : objects_to_be_removed) {
        ddgi_volumes_data_.erase(id);
    }
}

auto DdgiContext::num_ddgi_volumes() const -> uint32_t {
    return ddgi_volumes_data_.size();
}

auto DdgiContext::for_each_ddgi_volume(std::function<auto(DdgiVolumeInfo const&) -> void> func) const -> void {
    for (auto& [_, data] : ddgi_volumes_data_) {
        func(data);
    }
}

auto DdgiContext::set_irradiance_texture(gfx::TextureHandle irradiance) -> void {
    irradiance_texture_[db_ind_] = g_engine->graphics_manager()->render_graph().take_texture(irradiance);
    shader_data_[db_ind_].irradiance_texture = {irradiance_texture_[db_ind_].ref()};
}

auto DdgiContext::set_visibility_texture(gfx::TextureHandle visibility) -> void {
    visibility_texture_[db_ind_] = g_engine->graphics_manager()->render_graph().take_texture(visibility);
    shader_data_[db_ind_].visibility_texture = {visibility_texture_[db_ind_].ref()};
}

auto DdgiContext::init_sample_randoms() -> void {
    if (sample_randoms_buffer_.has_value()) { return; }

    constexpr auto ddgi_sample_randoms_size = 8192u;
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

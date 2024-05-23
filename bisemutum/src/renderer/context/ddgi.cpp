#include "ddgi.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>

namespace bi {

auto DdgiContext::update_frame() -> void {
    init_sample_randoms();

    auto scene = g_engine->world()->current_scene().value();

    std::unordered_set<rt::SceneObject::Id> objects_to_be_removed;
    objects_to_be_removed.reserve(ddgi_volumes_data_.size());
    for (auto& [id, _] : ddgi_volumes_data_) {
        objects_to_be_removed.insert(id);
    }

    auto ddgi_volumes_view = scene->ecs_registry().view<DdgiVolumeComponent>();
    uint32_t index = 0;
    for (auto entity : ddgi_volumes_view) {
        auto& ddgi_volume = ddgi_volumes_view.get<DdgiVolumeComponent>(entity);
        auto object = scene->object_of(entity);
        auto id = object->get_id();
        objects_to_be_removed.erase(id);

        auto [ddgi_data_it, _] = ddgi_volumes_data_.try_emplace(id);

        auto& transform = object->world_transform();
        ddgi_data_it->second.prev_index = ddgi_data_it->second.index;
        ddgi_data_it->second.index = index++;
        ddgi_data_it->second.base_position = transform.transform_position({-1.0f, -1.0f, -1.0f});
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

auto DdgiContext::for_each_ddgi_volume(std::function<auto(DdgiVolumeData const&) -> void> func) const -> void {
    for (auto& [_, data] : ddgi_volumes_data_) {
        func(data);
    }
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

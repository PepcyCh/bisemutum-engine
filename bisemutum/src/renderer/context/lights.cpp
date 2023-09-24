#include "lights.hpp"

#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/ecs.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/scene_basic/light.hpp>

namespace bi {

auto LightsContext::collect_all_lights() -> void {
    dir_lights.clear();
    point_lights.clear();
    spot_lights.clear();

    auto lights_view = g_engine->ecs_manager()->ecs_registry().view<LightComponent>();
    for (auto entity : lights_view) {
        auto object = g_engine->ecs_manager()->scene_object_of(entity);
        auto& transform = object->world_transform();
        auto& light = lights_view.get<LightComponent>(entity);
        LightData data{};
        data.emission = light.color * light.strength;
        switch (light.type) {
            case LightType::directional:
                data.direction = transform.transform_direction({0.0f, -1.0f, 0.0f});
                dir_lights.push_back(data);
                break;
            case LightType::point:
                data.position = transform.transform_position({0.0f, 0.0f, 0.0f});
                data.range_sqr = light.range * light.range;
                point_lights.push_back(data);
                break;
            case LightType::spot:
                data.position = transform.transform_position({0.0f, 0.0f, 0.0f});
                data.direction = transform.transform_direction({0.0f, -1.0f, 0.0f});
                data.cos_outer = std::cos(light.spot_outer_angle);
                data.cos_inner = std::cos(light.spot_inner_angle);
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
}

}

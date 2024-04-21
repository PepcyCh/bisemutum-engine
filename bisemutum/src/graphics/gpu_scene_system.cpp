#include <bisemutum/graphics/gpu_scene_system.hpp>

#include <bisemutum/containers/slotmap.hpp>
#include <bisemutum/containers/continuous_set.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/graphics/drawable.hpp>

#include "gpu_scene_data.hpp"

namespace bi::gfx {

struct GpuSceneSystem::Impl final {
    auto init_on(Ref<rt::Scene> scene) -> void {
        shader_parameter.initialize();
    }

    auto update() -> void {
        std::vector<float4x4> linear_history_transforms(drawables.capacity(), float4x4{1.0f});
        for (auto& [handle, transform] : history_transforms) {
            linear_history_transforms[static_cast<size_t>(handle)] = transform;
        }
        auto history_transforms_buffer_size = linear_history_transforms.size() * sizeof(float4x4);
        if (history_transforms_buffer_size > 0) {
            if (!history_transforms_buffer.has_value() || history_transforms_buffer.desc().size < history_transforms_buffer_size) {
                history_transforms_buffer = Buffer{rhi::BufferDesc{
                    .size = history_transforms_buffer_size,
                    .usages = {rhi::BufferUsage::storage_read},
                }};
            }
            history_transforms_buffer.set_data_immediately(linear_history_transforms.data(), linear_history_transforms.size());
        }
    }

    auto post_update() -> void {
        for (auto [handle, drawable] : drawables.pairs()) {
            history_transforms[handle] = drawable->transform.matrix();
        }
    }

    auto add_camera() -> CameraHandle {
        return cameras.emplace();
    }
    auto remove_camera(CameraHandle handle) -> void {
        cameras.remove(handle);
    }
    auto get_camera(CameraHandle handle) -> Ref<Camera> {
        return cameras.get(handle);
    }
    auto for_each_camera(std::function<auto(Camera&) -> void>&& func) -> void {
        for (auto& camera : cameras) {
            func(camera);
        }
    }
    auto for_each_camera(std::function<auto(Camera const&) -> void>&& func) const -> void {
        for (auto const& camera : cameras) {
            func(camera);
        }
    }

    auto add_drawable() -> DrawableHandle {
        auto handle = drawables.emplace();
        auto& drawable = drawables.get(handle);
        drawable.handle_ = handle;
        drawables_continuous_indices.insert(handle);
        return handle;
    }
    auto remove_drawable(DrawableHandle handle) -> void {
        drawables.remove(handle);
        drawables_continuous_indices.erase(handle);
        history_transforms.erase(handle);
    }
    auto get_drawable(DrawableHandle handle) -> Ref<Drawable> {
        return drawables.get(handle);
    }
    auto get_drawable(DrawableHandle handle) const -> CRef<Drawable> {
        return drawables.get(handle);
    }

    auto get_drawables_hash() -> size_t {
        auto frame_count = g_engine->window()->frame_count();
        if (drawables_hash_frame_count != frame_count) {
            drawables_hash = hash(drawables.size());
            for (auto const& drawable : drawables) {
                drawables_hash = hash_combine(
                    drawables_hash,
                    hash(drawable.mesh->mesh_type_name(), drawable.material ? drawable.material->get_shader_identifier() : "")
                );
            }
            drawables_hash_frame_count = frame_count;
        }
        return drawables_hash;
    }

    auto for_each_drawable(std::function<auto(Drawable&) -> void>&& func) -> void {
        for (auto& drawable : drawables) {
            func(drawable);
        }
    }
    auto for_each_drawable(std::function<auto(Drawable const&) -> void>&& func) const -> void {
        for (auto const& drawable : drawables) {
            func(drawable);
        }
    }
    auto for_each_drawable_with_shader_data(
        std::function<auto(Drawable&, DrawableShaderData const& drawable_data) -> void>&& func
    ) -> void {
        for (auto [handle, drawable] : drawables.pairs()) {
            func(*drawable, drawable_data_of(*drawable));
        }
    }
    auto for_each_drawable_with_shader_data(
        std::function<auto(Drawable const&, DrawableShaderData const& drawable_data) -> void>&& func
    ) const -> void {
        for (auto [handle, drawable] : drawables.pairs()) {
            func(*drawable, drawable_data_of(*drawable));
        }
    }

    auto drawable_data_of(Drawable const& drawable) const -> DrawableShaderData {
        float4x4 history_matrix_object_to_world(1.0f);
        if (auto history_it = history_transforms.find(drawable.handle()); history_it != history_transforms.end()) {
            history_matrix_object_to_world = history_it->second;
        }
        return DrawableShaderData{
            .matrix_object_to_world = drawable.transform.matrix(),
            .matrix_world_to_object_transposed = drawable.transform.matrix_transposed_inverse(),
            .history_matrix_object_to_world = history_matrix_object_to_world,
        };
    }

    auto update_shader_params() -> void {
        auto gpu_scene_data = shader_parameter.mutable_typed_data();
        gpu_scene_data->history_transforms_buffer = {&history_transforms_buffer};
        g_engine->graphics_manager()->fill_gpu_scene_data(*gpu_scene_data);
        shader_parameter.update_uniform_buffer();
    }

    SlotMap<Camera, CameraHandle> cameras;
    SlotMap<Drawable, DrawableHandle> drawables;
    ContinuousSet<DrawableHandle> drawables_continuous_indices;

    std::unordered_map<DrawableHandle, float4x4> history_transforms;
    Buffer history_transforms_buffer;

    size_t drawables_hash = 0;
    uint64_t drawables_hash_frame_count = static_cast<uint64_t>(-1);

    TShaderParameter<GpuSceneData> shader_parameter;
};

GpuSceneSystem::GpuSceneSystem() = default;
GpuSceneSystem::~GpuSceneSystem() = default;

GpuSceneSystem::GpuSceneSystem(GpuSceneSystem&& rhs) noexcept = default;
auto GpuSceneSystem::operator=(GpuSceneSystem&& rhs) noexcept -> GpuSceneSystem& = default;

auto GpuSceneSystem::init_on(Ref<rt::Scene> scene) -> void {
    impl()->init_on(scene);
}
auto GpuSceneSystem::update() -> void {
    impl()->update();
}
auto GpuSceneSystem::post_update() -> void {
    impl()->post_update();
}

auto GpuSceneSystem::add_camera() -> CameraHandle {
    return impl()->add_camera();
}
auto GpuSceneSystem::remove_camera(CameraHandle handle) -> void {
    impl()->remove_camera(handle);
}
auto GpuSceneSystem::get_camera(CameraHandle handle) -> Ref<Camera> {
    return impl()->get_camera(handle);
}
auto GpuSceneSystem::for_each_camera(std::function<auto(Camera&) -> void> func) -> void {
    impl()->for_each_camera(std::move(func));
}
auto GpuSceneSystem::for_each_camera(std::function<auto(Camera const&) -> void> func) const -> void {
    impl()->for_each_camera(std::move(func));
}

auto GpuSceneSystem::add_drawable() -> DrawableHandle {
    return impl()->add_drawable();
}
auto GpuSceneSystem::remove_drawable(DrawableHandle handle) -> void {
    impl()->remove_drawable(handle);
}
auto GpuSceneSystem::get_drawable(DrawableHandle handle) -> Ref<Drawable> {
    return impl()->get_drawable(handle);
}
auto GpuSceneSystem::get_drawable(DrawableHandle handle) const -> CRef<Drawable> {
    return impl()->get_drawable(handle);
}

auto GpuSceneSystem::drawables_hash() -> size_t {
    return impl()->get_drawables_hash();
}

auto GpuSceneSystem::num_drawables() const -> size_t {
    return impl()->drawables.size();
}
auto GpuSceneSystem::get_all_drawables() -> std::vector<Ref<Drawable>> {
    std::vector<Ref<Drawable>> drawables{};
    drawables.reserve(num_drawables());
    for_each_drawable([&drawables](Drawable& drawable) {
        drawables.push_back(drawable);
    });
    return drawables;
}
auto GpuSceneSystem::for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void {
    impl()->for_each_drawable(std::move(func));
}
auto GpuSceneSystem::for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void {
    impl()->for_each_drawable(std::move(func));
}
auto GpuSceneSystem::for_each_drawable_with_shader_data(
    std::function<auto(Drawable&, DrawableShaderData const& drawable_data) -> void> func
) -> void {
    impl()->for_each_drawable_with_shader_data(std::move(func));
}
auto GpuSceneSystem::for_each_drawable_with_shader_data(
    std::function<auto(Drawable const&, DrawableShaderData const& drawable_data) -> void> func
) const -> void {
    impl()->for_each_drawable_with_shader_data(std::move(func));
}

auto GpuSceneSystem::drawable_data_of(DrawableHandle handle) const -> DrawableShaderData {
    return impl()->drawable_data_of(*get_drawable(handle));
}
auto GpuSceneSystem::drawable_data_of(Drawable const& drawable) const -> DrawableShaderData {
    return impl()->drawable_data_of(drawable);
}

auto GpuSceneSystem::drawable_continuous_index_of(DrawableHandle handle) const -> size_t {
    return impl()->drawables_continuous_indices.index_of(handle);
}
auto GpuSceneSystem::drawable_continuous_index_of(Drawable const& drawable) const -> size_t {
    return impl()->drawables_continuous_indices.index_of(drawable.handle());
}

auto GpuSceneSystem::shader_params() -> ShaderParameter& {
    return impl()->shader_parameter;
}
auto GpuSceneSystem::shader_params_metadata() const -> ShaderParameterMetadataList const& {
    return impl()->shader_parameter.metadata_list();
}
auto GpuSceneSystem::update_shader_params() -> void {
    impl()->update_shader_params();
}

}

#include <bisemutum/graphics/displayer.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/graphics/camera.hpp>

namespace bi::gfx {

auto BlitDisplayer::display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<Texture> target_texture) -> void {
    if (displayed_camera_ != CameraHandle::invalid) {
        auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<GpuSceneSystem>();
        g_engine->graphics_manager()->blit_texture_2d(
            cmd_encoder,
            gpu_scene->get_camera(displayed_camera_)->target_texture(),
            target_texture
        );
    }
}

auto BlitDisplayer::is_valid() const -> bool {
    return displayed_camera_ != CameraHandle::invalid;
}

auto BlitDisplayer::set_camera(CameraHandle camera) -> void {
    displayed_camera_ = camera;
}

}

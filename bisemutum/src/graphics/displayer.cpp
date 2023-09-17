#include <bisemutum/graphics/displayer.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/camera.hpp>

namespace bi::gfx {

auto BlitDisplayer::display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<Texture> target_texture) -> void {
    if (displayed_camera_ != CameraHandle::invalid) {
        g_engine->graphics_manager()->blit_texture_2d(
            cmd_encoder,
            g_engine->graphics_manager()->get_camera(displayed_camera_)->target_texture(),
            target_texture
        );
    }
}

auto BlitDisplayer::set_camera(CameraHandle camera) -> void {
    displayed_camera_ = camera;
}

}

#include "register.hpp"

#include <bisemutum/graphics/graphics_manager.hpp>

#include <bisemutum/renderer/forward.hpp>

namespace bi {

auto register_renderers(Ref<gfx::GraphicsManager> mgr) -> void {
    mgr->register_renderer<ForwardRenderer>();
}

}

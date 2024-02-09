#pragma once

#include "shader.hpp"
#include "camera.hpp"
#include "drawable.hpp"

namespace bi::gfx {

enum class RenderedObjectType : uint8_t {
    opaque = 1,
    transparent = 2,
    all = opaque | transparent,
};
struct RenderedObjectListDesc final {
    CRef<Camera> camera;
    CRef<FragmentShader> fragment_shader;
    BitFlags<RenderedObjectType> type = {RenderedObjectType::all};
};

// Drawables those can use the same pipline state and vertex buffer.
struct RenderedObjectListItem final {
    std::vector<Ref<Drawable>> drawables;
};

struct RenderedObjectList final {
    CRef<Camera> camera;
    CRef<FragmentShader> fragment_shader;
    std::vector<RenderedObjectListItem> items;
};

}

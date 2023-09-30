#pragma once

#include "shader.hpp"
#include "camera.hpp"
#include "drawable.hpp"
#include "../rhi/pipeline.hpp"

namespace bi::gfx {

enum class RenderedObjectType : uint8_t {
    opaque = 1,
    transparent = 2,
    all = opaque | transparent,
};
struct RenderedObjectListDesc final {
    CRef<Camera> camera;
    CRef<FragmentShader> fragmeng_shader;
    BitFlags<RenderedObjectType> type = {RenderedObjectType::all};
};

struct RenderedObjectListItem final {
    std::vector<Ref<Drawable>> drawables;
};

struct RenderedObjectList final {
    CRef<Camera> camera;
    CRef<FragmentShader> fragmeng_shader;
    std::vector<RenderedObjectListItem> items;
};

}

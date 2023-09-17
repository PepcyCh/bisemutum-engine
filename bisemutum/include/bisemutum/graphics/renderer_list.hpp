#pragma once

#include "camera.hpp"
#include "drawable.hpp"
#include "shader.hpp"

namespace bi::gfx {

struct RenderedObjectListItem final {
    Ref<rhi::GraphicsPipeline> pipeline;
    std::vector<Ref<Drawable>> drawables;
};

struct RenderedObjectList final {
    CRef<Camera> camera;
    std::vector<RenderedObjectListItem> items;
};

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

}

#pragma once

#include "shader.hpp"
#include "camera.hpp"
#include "drawable.hpp"
#include "../prelude/span.hpp"

namespace bi::gfx {

enum class RenderedObjectType : uint8_t {
    opaque = 1,
    transparent = 2,
    all = opaque | transparent,
};
enum class RendererObjectSortingMode : uint8_t {
    none,
    from_front_to_back,
    from_back_to_front,
};
struct RenderedObjectListDesc final {
    CRef<Camera> camera;
    CRef<FragmentShader> fragment_shader;
    BitFlags<RenderedObjectType> type = {RenderedObjectType::all};
    // Result will be a subset of `candidate_drawables`
    Span<Ref<Drawable>> candidate_drawables;
    bool do_frustum_culling = true;
    RendererObjectSortingMode sorting_mode = RendererObjectSortingMode::from_front_to_back;
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

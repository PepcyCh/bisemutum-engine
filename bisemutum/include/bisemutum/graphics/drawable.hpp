#pragma once

#include "mesh.hpp"
#include "material.hpp"
#include "../math/transform.hpp"

namespace bi::gfx {

struct Drawable final {
    auto bounding_box() const -> BoundingBox { return transform.transform_bounding_box(mesh->bounding_box()); }

    Dyn<IMesh>::Ptr mesh;
    Ptr<Material> material;
    Transform transform;
};

}

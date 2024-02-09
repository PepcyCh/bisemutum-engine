#include <bisemutum/graphics/drawable.hpp>

namespace bi::gfx {

auto Drawable::bounding_box() const -> BoundingBox {
    return transform.transform_bounding_box(mesh->get_mesh_data().submesh_bounding_box(submesh_index));
}

auto Drawable::submesh_desc() const -> SubmeshDesc const& {
    return mesh->get_mesh_data().get_submesh(submesh_index);
}

}

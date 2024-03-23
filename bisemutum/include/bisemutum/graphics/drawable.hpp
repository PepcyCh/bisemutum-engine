#pragma once

#include "mesh.hpp"
#include "material.hpp"
#include "../math/transform.hpp"

namespace bi::gfx {

struct Drawable final {
    auto bounding_box() const -> BoundingBox;
    auto submesh_desc() const -> SubmeshDesc const&;

    Dyn<IMesh>::Ptr mesh;
    uint32_t submesh_index = 0;
    // `material` can be a nullptr in some special cases.
    Ptr<Material> material;
    Transform transform;

    ShaderParameter shader_params;
};

BI_SHADER_PARAMETERS_BEGIN(DrawableShaderData)
    BI_SHADER_PARAMETER(float4x4, matrix_object_to_world)
    BI_SHADER_PARAMETER(float4x4, matrix_world_to_object_transposed)
    BI_SHADER_PARAMETER(float4x4, history_matrix_object_to_world)
BI_SHADER_PARAMETERS_END()

}

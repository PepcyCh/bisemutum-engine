#pragma once

#include <bisemutum/math/bbox.hpp>
#include <bisemutum/graphics/drawable.hpp>
#include <bisemutum/graphics/shader_param.hpp>
#include <bisemutum/graphics/vertex_attributes_type.hpp>
#include <bisemutum/rhi/pipeline.hpp>
#include <bisemutum/rhi/command.hpp>

namespace bi::gfx {

struct ScreenTriangle final {
    BI_SHADER_PARAMETERS_BEGIN(ShaderParams)
    BI_SHADER_PARAMETERS_END(ShaderParams)

    ScreenTriangle() {
        mesh_.set_submesh(0, {.num_indices = 3});
    }

    auto mesh_type_name() const -> std::string_view { return "ScreenTriangle"; }
    auto get_mesh_data() const -> gfx::MeshData const& { return mesh_; }
    auto get_mutable_mesh_data() -> gfx::MeshData& { return mesh_; }
    auto fill_shader_params(Ref<Drawable> drawable, DrawableShaderData const& drawable_data) const -> void {}
    auto shader_params_metadata(uint32_t submesh_index) const -> ShaderParameterMetadataList const& {
        return ShaderParams::metadata_list();
    }
    auto source_path(rhi::ShaderStage stage) const -> std::string_view {
        if (stage == rhi::ShaderStage::vertex) {
            return "/bisemutum/shaders/core/screen_triangle.hlsl";
        } else {
            return "";
        }
    }
    auto source_entry(rhi::ShaderStage stage) const -> std::string_view {
        if (stage == rhi::ShaderStage::vertex) {
            return "screen_triangle_vs";
        } else {
            return "";
        }
    }

private:
    MeshData mesh_;
};

}

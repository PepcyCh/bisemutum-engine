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
    auto bounding_box() const -> BoundingBox { return {}; }
    auto vertex_input_desc(VertexAttributesType attributes_type) const -> std::vector<rhi::VertexInputBufferDesc> {
        return {};
    }
    auto num_indices() const -> uint32_t { return 3; }
    auto bind_buffers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder) -> void {}
    auto fill_shader_params(Ref<Drawable> drawable) const -> void {}
    auto shader_params_metadata() const -> ShaderParameterMetadataList const& { return ShaderParams::metadata_list(); }
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

#pragma once

#include <string_view>

#include "../math/bbox.hpp"
#include "../runtime/asset.hpp"
#include "../graphics/shader_param.hpp"
#include "../graphics/drawable.hpp"
#include "../graphics/vertex_attributes_type.hpp"
#include "../rhi/command.hpp"
#include "../rhi/pipeline.hpp"

namespace bi {

struct StaticMesh final {
    static constexpr std::string_view asset_type_name = "StaticMesh";

    // -- For TAsset --
    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

    auto save(Dyn<rt::IFile>::Ref file) const -> void;

    // -- For IMesh --
    BI_SHADER_PARAMETERS_BEGIN(ShaderParams)
        BI_SHADER_PARAMETER(float4x4, matrix_object_to_world)
        BI_SHADER_PARAMETER(float4x4, matrix_world_to_object_transposed)
    BI_SHADER_PARAMETERS_END(ShaderParams)

    auto mesh_type_name() const -> std::string_view { return asset_type_name; }
    auto bounding_box() const -> BoundingBox { return bbox_; }
    auto vertex_input_desc(gfx::VertexAttributesType attributes_type) const -> std::vector<rhi::VertexInputBufferDesc>;
    auto num_indices() const -> uint32_t { return indices_.size(); }
    auto bind_buffers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder) -> void;
    auto fill_shader_params(Ref<gfx::Drawable> drawable) const -> void;
    auto shader_params_metadata() const -> gfx::ShaderParameterMetadataList const& {
        return ShaderParams::metadata_list();
    }
    auto source_path(rhi::ShaderStage stage) const -> std::string_view {
        if (stage == rhi::ShaderStage::vertex) {
            return "/bisemutum/shaders/scene_basic/static_mesh.hlsl";
        } else {
            return "";
        }
    }
    auto source_entry(rhi::ShaderStage stage) const -> std::string_view {
        if (stage == rhi::ShaderStage::vertex) {
            return "static_mesh_vs";
        } else {
            return "";
        }
    }

    auto positions() const -> CSpan<float3> { return positions_; }
    auto normals() const -> CSpan<float3> { return normals_; }
    auto tangents() const -> CSpan<float4> { return tangents_; }
    auto texcoords() const -> CSpan<float2> { return texcoords_; }
    auto indices() const -> CSpan<uint32_t> { return indices_; }

    auto position_at(size_t index) const -> float3 const& { return positions_[index]; }
    auto normal_at(size_t index) const -> float3 const& { return normals_[index]; }
    auto tangent_at(size_t index) const -> float4 const& { return tangents_[index]; }
    auto texcoord_at(size_t index) const -> float2 const& { return texcoords_[index]; }
    auto index_at(size_t index) const -> uint32_t { return indices_[index]; }

    auto resize(size_t num_vertices, size_t num_indices) -> void;
    auto set_position_at(size_t index, float3 const& data) -> void;
    auto set_normal_at(size_t index, float3 const& data) -> void;
    auto set_tangent_at(size_t index, float4 const& data) -> void;
    auto set_texcoord_at(size_t index, float2 const& data) -> void;
    auto set_index_at(size_t index, uint32_t data) -> void;

    auto set_positions_raw(float3 const* data) -> void;
    auto set_normals_raw(float3 const* data) -> void;
    auto set_tangents_raw(float4 const* data) -> void;
    auto set_texcoords_raw(float2 const* data) -> void;
    auto set_indices_raw(uint32_t const* data) -> void;
    auto set_bouding_box_raw(float3 const& p_min, float3 const& p_max) -> void;

    auto calculate_tspace() -> void;

    auto update_gpu_buffer() -> void;

private:
    std::vector<float3> positions_;
    std::vector<float3> normals_;
    std::vector<float4> tangents_;
    std::vector<float2> texcoords_;
    std::vector<uint32_t> indices_;
    BoundingBox bbox_;

    gfx::Buffer positions_buffer_;
    gfx::Buffer normals_buffer_;
    gfx::Buffer tangents_buffer_;
    gfx::Buffer texcoords_buffer_;
    gfx::Buffer indices_buffer_;
    bool positions_dirty_ = true;
    bool normals_dirty_ = true;
    bool tangents_dirty_ = true;
    bool texcoords_dirty_ = true;
    bool indices_dirty_ = true;
};

struct StaticMeshComponent final {
    static constexpr std::string_view component_type_name = "StaticMeshComponent";

    rt::TAssetPtr<StaticMesh> static_mesh;
};
BI_SREFL(type(StaticMeshComponent), field(static_mesh))

}

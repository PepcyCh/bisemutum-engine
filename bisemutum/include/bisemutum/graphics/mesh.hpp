#pragma once

#include "shader_source.hpp"
#include "shader_param.hpp"
#include "shader_compilation_environment.hpp"
#include "../prelude/poly.hpp"
#include "../prelude/byte_stream.hpp"
#include "../math/bbox.hpp"
#include "../rhi/pipeline.hpp"

namespace bi::gfx {

struct Drawable;
struct DrawableShaderData;

struct SubmeshDesc final {
    uint32_t base_vertex = 0;
    uint32_t index_offset = 0;
    // If index data is empty, `num_indices` means number of vertices.
    uint32_t num_indices = 0;
    rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::triangle_list;
};

struct MeshData final {
    MeshData();

    auto num_vertices() const -> uint32_t { return static_cast<uint32_t>(positions_.size()); }
    auto num_indices() const -> uint32_t { return static_cast<uint32_t>(indices_.size()); }

    auto positions() const -> std::vector<float3> const& { return positions_; }
    auto mutable_positions() -> std::vector<float3>&;
    auto has_position() const -> bool { return !positions_.empty(); }

    auto normals() const -> std::vector<float3> const& { return normals_; }
    auto mutable_normals() -> std::vector<float3>&;
    auto has_normal() const -> bool { return !normals_.empty(); }

    auto tangents() const -> std::vector<float4> const& { return tangents_; }
    auto mutable_tangents() -> std::vector<float4>&;
    auto has_tangent() const -> bool { return !tangents_.empty(); }

    auto colors() const -> std::vector<float3> const& { return colors_; }
    auto mutable_colors() -> std::vector<float3>&;
    auto has_color() const -> bool { return !colors_.empty(); }

    auto texcoords() const -> std::vector<float2> const& { return texcoords_; }
    auto mutable_texcoords() -> std::vector<float2>&;
    auto has_texcoord() const -> bool { return !texcoords_.empty(); }

    auto texcoords2() const -> std::vector<float2> const& { return texcoords2_; }
    auto mutable_texcoords2() -> std::vector<float2>&;
    auto has_texcoord2() const -> bool { return !texcoords2_.empty(); }

    auto indices() const -> std::vector<uint32_t> const& { return indices_; }
    auto mutable_indices() -> std::vector<uint32_t>&;

    auto num_submehes() const -> uint32_t { return static_cast<uint32_t>(submeshes_.size()); }
    auto set_submehes(std::vector<SubmeshDesc> submeshes) -> void;
    auto set_submesh(uint32_t index, SubmeshDesc const& submesh) -> void;
    auto get_submesh(uint32_t index) const -> SubmeshDesc const& { return submeshes_[index]; }

    auto bounding_box() const -> BoundingBox const&;
    auto submesh_bounding_box(uint32_t index) const -> BoundingBox const&;

    auto save_to_byte_stream(WriteByteStream& bs) const -> void;
    auto load_from_byte_stream(ReadByteStream& bs) -> void;

private:
    auto set_buffer_dirty() -> void;
    auto set_geometry_dirty() -> void;
    auto set_submesh_dirty(uint32_t index) -> void;

    auto get_submesh_version(uint32_t index) const -> uint64_t;

    std::vector<float3> positions_;
    std::vector<float3> normals_;
    std::vector<float4> tangents_;
    std::vector<float3> colors_;
    std::vector<float2> texcoords_;
    std::vector<float2> texcoords2_;

    std::vector<uint32_t> indices_;

    std::vector<SubmeshDesc> submeshes_;

    mutable BoundingBox bbox_;
    mutable std::vector<BoundingBox> submesh_bboxes_;

    friend GraphicsManager;
    static uint64_t curr_id_;
    const uint64_t id_;
    uint64_t buffer_version_ = 1;
    uint64_t geometry_version_ = 1;
    std::vector<uint64_t> submesh_versions_;
};

BI_TRAIT_BEGIN(IMesh, move)
    template <typename T>
    static auto helper_tessellation_desc(T const& self) -> rhi::TessellationState { return {}; }
    template <typename T> requires requires (const T v) { v.tessellation_desc(); }
    static auto helper_tessellation_desc(T const& self) -> rhi::TessellationState {
        return self.tessellation_desc();
    }

    template <typename T>
    static auto helper_modify_compiler_environment(
        T const& self, ShaderCompilationEnvironment& compilation_environment
    ) -> void {}
    template <typename T> requires requires (const T v) { v.modify_compiler_environment(); }
    static auto helper_modify_compiler_environment(
        T const& self, ShaderCompilationEnvironment& compilation_environment
    ) -> void {
        return self.modify_compiler_environment();
    }

    BI_TRAIT_METHOD(mesh_type_name, (const& self) requires (self.mesh_type_name()) -> std::string_view)

    BI_TRAIT_METHOD(get_mesh_data, (const& self) requires (self.get_mesh_data()) -> MeshData const&)
    BI_TRAIT_METHOD(get_mutable_mesh_data, (&self) requires (self.get_mutable_mesh_data()) -> MeshData&)

    BI_TRAIT_METHOD(tessellation_desc,
        (const& self) requires (helper_tessellation_desc(self)) -> rhi::TessellationState
    )

    BI_TRAIT_METHOD(fill_shader_params,
        (const& self, Ref<Drawable> drawable, DrawableShaderData const& drawable_data)
            requires (self.fill_shader_params(drawable, drawable_data)) -> void
    )
    BI_TRAIT_METHOD(shader_params_metadata,
        (const& self, uint32_t submesh_index)
            requires (self.shader_params_metadata(submesh_index)) -> ShaderParameterMetadataList const&
    )

    BI_TRAIT_METHOD(source,
        (const& self, rhi::ShaderStage stage) requires (self.source(stage)) -> ShaderSource
    )
    BI_TRAIT_METHOD(modify_compiler_environment,
        (const& self, ShaderCompilationEnvironment& compilation_environment)
            requires (helper_modify_compiler_environment(self, compilation_environment)) -> void
    )
BI_TRAIT_END(IMesh)

}

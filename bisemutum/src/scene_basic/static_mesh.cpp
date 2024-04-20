#include <bisemutum/scene_basic/static_mesh.hpp>

#include <bisemutum/prelude/byte_stream.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <mikktspace.h>

namespace bi {

auto StaticMesh::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    auto binary_data = file.read_binary_data();
    ReadByteStream bs{binary_data};

    uint32_t magic_number = 0;
    std::string asset_type_name;
    uint32_t version = 0;
    bs.read(magic_number).read(asset_type_name).read(version);
    if (!rt::check_if_binary_asset_valid<StaticMesh>(file.filename(), magic_number, asset_type_name)) {
        return {};
    }

    StaticMesh mesh{};
    if (version == 1) {
        mesh.mesh_.load_from_byte_stream(bs);
    } else if (version == 2) {
        ReadByteStream data_bs{};
        bs.read_compressed_part(data_bs);
        mesh.mesh_.load_from_byte_stream(data_bs);
    }

    return mesh;
}

auto StaticMesh::save(Dyn<rt::IFile>::Ref file) const -> void {
    WriteByteStream bs{};
    bs.write(rt::asset_magic_number).write(StaticMesh::asset_type_name).write(2u);

    auto data_from = bs.curr_offset();
    mesh_.save_to_byte_stream(bs);
    bs.compress_data(data_from);

    file.write_binary_data(bs.data());
}

auto StaticMesh::fill_shader_params(
    Ref<gfx::Drawable> drawable, gfx::DrawableShaderData const& drawable_data
) const -> void {
    auto data = drawable->shader_params.mutable_typed_data<ShaderParams>();
    data->drawable_data = drawable_data;
}

auto StaticMesh::resize(size_t num_vertices, size_t num_indices) -> void {
    mesh_.mutable_positions().resize(num_vertices);
    mesh_.mutable_normals().resize(num_vertices);
    mesh_.mutable_tangents().resize(num_vertices);
    mesh_.mutable_texcoords().resize(num_vertices);
    mesh_.mutable_indices().resize(num_indices);
}
auto StaticMesh::set_position_at(size_t index, float3 const& data) -> void {
    mesh_.mutable_positions()[index] = data;
}
auto StaticMesh::set_normal_at(size_t index, float3 const& data) -> void {
    mesh_.mutable_normals()[index] = data;
}
auto StaticMesh::set_tangent_at(size_t index, float4 const& data) -> void {
    mesh_.mutable_tangents()[index] = data;
}
auto StaticMesh::set_texcoord_at(size_t index, float2 const& data) -> void {
    mesh_.mutable_texcoords()[index] = data;
}
auto StaticMesh::set_index_at(size_t index, uint32_t data) -> void {
    mesh_.mutable_indices()[index] = data;
}

auto StaticMesh::set_positions_raw(float3 const* data) -> void {
    std::copy_n(data, mesh_.positions().size(), mesh_.mutable_positions().data());
}
auto StaticMesh::set_normals_raw(float3 const* data) -> void {
    std::copy_n(data, mesh_.normals().size(), mesh_.mutable_normals().data());
}
auto StaticMesh::set_tangents_raw(float4 const* data) -> void {
    std::copy_n(data, mesh_.tangents().size(), mesh_.mutable_tangents().data());
}
auto StaticMesh::set_texcoords_raw(float2 const* data) -> void {
    std::copy_n(data, mesh_.texcoords().size(), mesh_.mutable_texcoords().data());
}
auto StaticMesh::set_indices_raw(uint32_t const* data) -> void {
    std::copy_n(data, mesh_.indices().size(), mesh_.mutable_indices().data());
}

auto StaticMesh::calculate_tspace() -> void {
    struct MikkTSpaceUserData final {
        StaticMesh* mesh;
        gfx::SubmeshDesc const* submesh;
    };

    mesh_.mutable_tangents().resize(mesh_.positions().size());
    for (uint32_t i = 0; i < mesh_.num_submehes(); i++) {
        auto& submesh = mesh_.get_submesh(i);
        MikkTSpaceUserData user_data{
            .mesh = this,
            .submesh = &submesh,
        };
        SMikkTSpaceInterface mikktspace_interface{
            .m_getNumFaces = [](SMikkTSpaceContext const* ctx) {
                auto data = static_cast<MikkTSpaceUserData const*>(ctx->m_pUserData);
                auto num_indices = data->submesh->num_indices;
                if (data->submesh->num_indices == ~0u) {
                    num_indices = data->mesh->indices().size() - data->submesh->index_offset;
                }
                return static_cast<int>(num_indices / 3);
            },
            .m_getNumVerticesOfFace = [](SMikkTSpaceContext const*, const int) { return 3; },
            .m_getPosition = [](SMikkTSpaceContext const* ctx, float* pos_out, const int face, const int vert) {
                auto data = static_cast<MikkTSpaceUserData const*>(ctx->m_pUserData);
                auto index = data->submesh->base_vertex + data->mesh->index_at(data->submesh->index_offset + face * 3 + vert);
                auto& pos = data->mesh->position_at(index);
                pos_out[0] = pos.x;
                pos_out[1] = pos.y;
                pos_out[2] = pos.z;
            },
            .m_getNormal = [](SMikkTSpaceContext const* ctx, float* norm_out, const int face, const int vert) {
                auto data = static_cast<MikkTSpaceUserData const*>(ctx->m_pUserData);
                auto index = data->submesh->base_vertex + data->mesh->index_at(data->submesh->index_offset + face * 3 + vert);
                auto& norm = data->mesh->normal_at(index);
                norm_out[0] = norm.x;
                norm_out[1] = norm.y;
                norm_out[2] = norm.z;
            },
            .m_getTexCoord = [](SMikkTSpaceContext const* ctx, float* uv_out, const int face, const int vert) {
                auto data = static_cast<MikkTSpaceUserData const*>(ctx->m_pUserData);
                auto index = data->submesh->base_vertex + data->mesh->index_at(data->submesh->index_offset + face * 3 + vert);
                auto& uv = data->mesh->texcoord_at(index);
                uv_out[0] = uv.x;
                uv_out[1] = uv.y;
            },
            .m_setTSpaceBasic = [](
                SMikkTSpaceContext const* ctx, float const* tan_in, const float sign, const int face, const int vert
            ) {
                auto data = static_cast<MikkTSpaceUserData const*>(ctx->m_pUserData);
                auto index = data->submesh->base_vertex + data->mesh->index_at(data->submesh->index_offset + face * 3 + vert);
                data->mesh->set_tangent_at(index, float4{tan_in[0], tan_in[1], tan_in[2], sign});
            },
            .m_setTSpace = nullptr
        };
        SMikkTSpaceContext mikktspace_ctx{
            .m_pInterface = &mikktspace_interface,
            .m_pUserData = &user_data,
        };
        genTangSpaceDefault(&mikktspace_ctx);
    }
}

}

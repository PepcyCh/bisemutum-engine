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
    bs.read(mesh.bbox_);
    bs.read(mesh.positions_);
    bs.read(mesh.texcoords_);
    bs.read(mesh.normals_);
    bs.read(mesh.tangents_);
    bs.read(mesh.indices_);
    mesh.update_gpu_buffer();

    return mesh;
}

auto StaticMesh::save(Dyn<rt::IFile>::Ref file) const -> void {
    WriteByteStream bs{};
    bs.write(rt::asset_magic_number).write(StaticMesh::asset_type_name).write(1u);

    bs.write(bbox_);
    bs.write(positions_);
    bs.write(texcoords_);
    bs.write(normals_);
    bs.write(tangents_);
    bs.write(indices_);

    file.write_binary_data(bs.data());
}

auto StaticMesh::vertex_input_desc(
    gfx::VertexAttributesType attributes_type
) const -> std::vector<rhi::VertexInputBufferDesc> {
    auto descs = std::vector{
        rhi::VertexInputBufferDesc{
            .stride = sizeof(float3),
            .attributes = {
                rhi::VertexInputAttribute{
                    .semantics = rhi::VertexSemantics::position,
                    .format = rhi::ResourceFormat::rgb32_sfloat,
                }
            },
        },
    };
    if (attributes_type == gfx::VertexAttributesType::position_only) {
        return descs;
    }
    descs.reserve(4);
    descs.push_back(rhi::VertexInputBufferDesc{
        .stride = sizeof(float3),
        .attributes = {
            rhi::VertexInputAttribute{
                .semantics = rhi::VertexSemantics::normal,
                .format = rhi::ResourceFormat::rgb32_sfloat,
            }
        },
    });
    descs.push_back(rhi::VertexInputBufferDesc{
        .stride = sizeof(float4),
        .attributes = {
            rhi::VertexInputAttribute{
                .semantics = rhi::VertexSemantics::tangent,
                .format = rhi::ResourceFormat::rgba32_sfloat,
            }
        },
    });
    descs.push_back(rhi::VertexInputBufferDesc{
        .stride = sizeof(float2),
        .attributes = {
            rhi::VertexInputAttribute{
                .semantics = rhi::VertexSemantics::texcoord0,
                .format = rhi::ResourceFormat::rg32_sfloat,
            }
        },
    });
    return descs;
}

auto StaticMesh::fill_shader_params(Ref<gfx::Drawable> drawable) const -> void {
    auto data = drawable->shader_params.mutable_typed_data<ShaderParams>();
    data->matrix_object_to_world = drawable->transform.matrix();
    data->matrix_world_to_object_transposed = drawable->transform.matrix_transposed_inverse();
}

auto StaticMesh::bind_buffers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder) -> void {
    std::array<Ref<rhi::Buffer>, 4> vertex_buffers{
        positions_buffer_.rhi_buffer(),
        normals_buffer_.rhi_buffer(),
        tangents_buffer_.rhi_buffer(),
        texcoords_buffer_.rhi_buffer(),
    };
    cmd_encoder->set_vertex_buffer(vertex_buffers);
    cmd_encoder->set_index_buffer(indices_buffer_.rhi_buffer());
}

auto StaticMesh::resize(size_t num_vertices, size_t num_indices) -> void {
    positions_.resize(num_vertices);
    normals_.resize(num_vertices);
    tangents_.resize(num_vertices);
    texcoords_.resize(num_vertices);
    indices_.resize(num_indices);
}
auto StaticMesh::set_position_at(size_t index, float3 const& data) -> void {
    positions_[index] = data;
    positions_dirty_ = true;
}
auto StaticMesh::set_normal_at(size_t index, float3 const& data) -> void {
    normals_[index] = data;
    normals_dirty_ = true;
}
auto StaticMesh::set_tangent_at(size_t index, float4 const& data) -> void {
    tangents_[index] = data;
    tangents_dirty_ = true;
}
auto StaticMesh::set_texcoord_at(size_t index, float2 const& data) -> void {
    texcoords_[index] = data;
    texcoords_dirty_ = true;
}
auto StaticMesh::set_index_at(size_t index, uint32_t data) -> void {
    indices_[index] = data;
    indices_dirty_ = true;
}

auto StaticMesh::set_positions_raw(float3 const* data) -> void {
    std::copy_n(data, positions_.size(), positions_.data());
    positions_dirty_ = true;
}
auto StaticMesh::set_normals_raw(float3 const* data) -> void {
    std::copy_n(data, normals_.size(), normals_.data());
    normals_dirty_ = true;
}
auto StaticMesh::set_tangents_raw(float4 const* data) -> void {
    std::copy_n(data, tangents_.size(), tangents_.data());
    tangents_dirty_ = true;
}
auto StaticMesh::set_texcoords_raw(float2 const* data) -> void {
    std::copy_n(data, texcoords_.size(), texcoords_.data());
    texcoords_dirty_ = true;
}
auto StaticMesh::set_indices_raw(uint32_t const* data) -> void {
    std::copy_n(data, indices_.size(), indices_.data());
    indices_dirty_ = true;
}
auto StaticMesh::set_bouding_box_raw(float3 const& p_min, float3 const& p_max) -> void {
    bbox_.p_min = p_min;
    bbox_.p_max = p_max;
}

auto StaticMesh::calculate_tspace() -> void {
    SMikkTSpaceInterface mikktspace_interface{
        .m_getNumFaces = [](SMikkTSpaceContext const* ctx) {
            auto mesh = static_cast<StaticMesh const*>(ctx->m_pUserData);
            return static_cast<int>(mesh->indices().size() / 3);
        },
        .m_getNumVerticesOfFace = [](SMikkTSpaceContext const*, const int) { return 3; },
        .m_getPosition = [](SMikkTSpaceContext const* ctx, float* pos_out, const int face, const int vert) {
            auto mesh = static_cast<StaticMesh const*>(ctx->m_pUserData);
            auto index = mesh->index_at(face * 3 + vert);
            auto& pos = mesh->position_at(index);
            pos_out[0] = pos.x;
            pos_out[1] = pos.y;
            pos_out[2] = pos.z;
        },
        .m_getNormal = [](SMikkTSpaceContext const* ctx, float* norm_out, const int face, const int vert) {
            auto mesh = static_cast<StaticMesh const*>(ctx->m_pUserData);
            auto index = mesh->index_at(face * 3 + vert);
            auto& norm = mesh->normal_at(index);
            norm_out[0] = norm.x;
            norm_out[1] = norm.y;
            norm_out[2] = norm.z;
        },
        .m_getTexCoord = [](SMikkTSpaceContext const* ctx, float* uv_out, const int face, const int vert) {
            auto mesh = static_cast<StaticMesh const*>(ctx->m_pUserData);
            auto index = mesh->index_at(face * 3 + vert);
            auto& uv = mesh->texcoord_at(index);
            uv_out[0] = uv.x;
            uv_out[1] = uv.y;
        },
        .m_setTSpaceBasic = [](
            SMikkTSpaceContext const* ctx, float const* tan_in, const float sign, const int face, const int vert
        ) {
            auto mesh = static_cast<StaticMesh*>(ctx->m_pUserData);
            auto index = mesh->index_at(face * 3 + vert);
            mesh->set_tangent_at(index, float4{tan_in[0], tan_in[1], tan_in[2], sign});
        },
        .m_setTSpace = nullptr
    };
    SMikkTSpaceContext mikktspace_ctx{
        .m_pInterface = &mikktspace_interface,
        .m_pUserData = this,
    };
    genTangSpaceDefault(&mikktspace_ctx);
}

auto StaticMesh::update_gpu_buffer() -> void {
    auto update_buffer = [](
        gfx::Buffer& buffer, auto const& cpu_data, bool& dirty,
        BitFlags<rhi::BufferUsage> usage = rhi::BufferUsage::vertex
    ) {
        if (!buffer.has_value()) {
            buffer = gfx::Buffer(
                gfx::BufferBuilder()
                    .size(cpu_data.size() * sizeof(cpu_data[0]))
                    .usage(usage)
            );
        }
        if (dirty) {
            buffer.set_data(cpu_data.data(), cpu_data.size());
            dirty = false;
        }
    };
    update_buffer(positions_buffer_, positions_, positions_dirty_);
    update_buffer(normals_buffer_, normals_, normals_dirty_);
    update_buffer(tangents_buffer_, tangents_, tangents_dirty_);
    update_buffer(texcoords_buffer_, texcoords_, texcoords_dirty_);
    update_buffer(indices_buffer_, indices_, indices_dirty_, rhi::BufferUsage::index);
}

}

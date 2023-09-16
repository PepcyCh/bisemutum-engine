#include <bisemutum/scene_basic/static_mesh.hpp>

#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <mikktspace.h>

namespace bi {

namespace {

auto generate_mikktspace(StaticMesh& mesh) -> void {
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
    };
    SMikkTSpaceContext mikktspace_ctx{
        .m_pInterface = &mikktspace_interface,
        .m_pUserData = &mesh,
    };
    genTangSpaceDefault(&mikktspace_ctx);
}

} // namespace

auto StaticMesh::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    auto file_data = file.read_binary_data();
    Assimp::Importer importer{};
    importer.SetPropertyInteger(
        AI_CONFIG_PP_RVC_FLAGS,
        aiComponent_LIGHTS
        | aiComponent_CAMERAS
        | aiComponent_COLORS
        | aiComponent_TEXTURES
        | aiComponent_MATERIALS
        | aiComponent_BONEWEIGHTS
        | aiComponent_ANIMATIONS
    );
    auto scene = importer.ReadFileFromMemory(
        file_data.data(), file_data.size(),
        aiProcess_Triangulate
        | aiProcess_SortByPType
        | aiProcess_GenNormals
        | aiProcess_GenUVCoords
        | aiProcess_JoinIdenticalVertices
        | aiProcess_PreTransformVertices
        | aiProcess_RemoveComponent
    );
    if (scene == nullptr) {
        log::critical("general", "Failed to load mesh file '{}': {}", file.filename(), importer.GetErrorString());
        return {};
    }
    if (scene->mNumMeshes != 1) {
        log::critical("general", "Failed to load mesh file '{}': Unexpected # meshes", file.filename());
        return {};
    }
    auto ai_mesh = scene->mMeshes[0];

    StaticMesh mesh{};
    mesh.positions_.resize(ai_mesh->mNumVertices);
    std::memcpy(mesh.positions_.data(), ai_mesh->mVertices, ai_mesh->mNumVertices * sizeof(float3));
    mesh.normals_.resize(ai_mesh->mNumVertices);
    std::memcpy(mesh.normals_.data(), ai_mesh->mNormals, ai_mesh->mNumVertices * sizeof(float3));
    mesh.texcoords_.resize(ai_mesh->mNumVertices);
    std::memcpy(mesh.texcoords_.data(), ai_mesh->mTextureCoords[0], ai_mesh->mNumVertices * sizeof(float3));
    mesh.indices_.resize(ai_mesh->mNumFaces * 3);
    for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++) {
        auto& ai_face = ai_mesh->mFaces[i];
        mesh.indices_[3 * i] = ai_face.mIndices[0];
        mesh.indices_[3 * i + 1] = ai_face.mIndices[1];
        mesh.indices_[3 * i + 2] = ai_face.mIndices[2];
    }
    generate_mikktspace(mesh);

    auto& aabb = ai_mesh->mAABB;
    mesh.bbox_.p_min = {aabb.mMin.x, aabb.mMin.y, aabb.mMin.z};
    mesh.bbox_.p_max = {aabb.mMax.x, aabb.mMax.y, aabb.mMax.z};

    return mesh;
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

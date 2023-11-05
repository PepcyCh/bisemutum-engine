#include <bisemutum/scene_basic/menu_actions.hpp>

#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/editor/file_dialog.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>

namespace bi::editor {

namespace {

auto menu_action_import_model_assimp(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Import Model (Assimp)", "Choose File", ".obj,.fbx",
        [](std::string choosed_path) {
            // TODO
            std::filesystem::path path{choosed_path};
            if (!std::filesystem::exists(path)) { return; }
            rt::PhysicalFile file{path, false};

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
                return;
            }
            if (scene->mNumMeshes != 1) {
                log::critical("general", "Failed to load mesh file '{}': Unexpected # meshes", file.filename());
                return;
            }
            auto ai_mesh = scene->mMeshes[0];

            StaticMesh mesh{};
            mesh.resize(ai_mesh->mNumVertices, ai_mesh->mNumFaces * 3);
            mesh.set_positions_raw(reinterpret_cast<float3 const*>(ai_mesh->mVertices));
            mesh.set_normals_raw(reinterpret_cast<float3 const*>(ai_mesh->mNormals));
            for (uint32_t i = 0; i < ai_mesh->mNumVertices; i++) {
                mesh.set_texcoord_at(i, {ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y});
            }
            for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++) {
                auto& ai_face = ai_mesh->mFaces[i];
                mesh.set_index_at(3 * i, ai_face.mIndices[0]);
                mesh.set_index_at(3 * i + 1, ai_face.mIndices[1]);
                mesh.set_index_at(3 * i + 2, ai_face.mIndices[2]);
            }
            mesh.calculate_tspace();

            auto& aabb = ai_mesh->mAABB;
            mesh.set_bouding_box_raw({aabb.mMin.x, aabb.mMin.y, aabb.mMin.z}, {aabb.mMax.x, aabb.mMax.y, aabb.mMax.z});

            mesh.update_gpu_buffer();
        }
    );
}

} // namespace

auto register_menu_actions_scene_basic(Ref<editor::MenuManager> mgr) -> void {
    mgr->register_action("Asset/Import/Model (Assimp)", {}, menu_action_import_model_assimp);
}

}

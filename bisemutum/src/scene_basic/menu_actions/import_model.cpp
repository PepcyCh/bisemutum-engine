#include "import_model.hpp"

#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/material.hpp>
#include <bisemutum/scene_basic/mesh_renderer.hpp>
#include <bisemutum/editor/file_dialog.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/runtime/prefab.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>

namespace bi::editor {

auto menu_action_import_model_assimp(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Import Model (Assimp)", "Choose File", ".obj,.fbx",
        [](std::string choosed_path) {
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
            if (scene->mNumMeshes == 0) {
                log::critical("general", "Failed to load mesh file '{}': No mesh can be loadded.", file.filename());
                return;
            }

            auto model_name = file.filename();
            model_name = model_name.substr(0, model_name.find('.'));

            auto current_scene = g_engine->world()->current_scene().value();
            auto object = current_scene->create_scene_object();
            object->set_name(model_name);
            auto curr_object = object;

            std::unordered_set<std::string> used_names{};
            std::vector<rt::AssetId> mat_ids{scene->mNumMaterials};
            for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
                auto ai_mat = scene->mMaterials[i];

                auto mat_name = std::string{ai_mat->GetName().data, ai_mat->GetName().length};
                if (mat_name.empty() || used_names.contains(mat_name)) {
                    mat_name = fmt::format("material{}", i);
                }
                used_names.insert(mat_name);

                auto mat_path = fmt::format("/project/imported/models/{}/{}.material.toml", model_name, mat_name);
                auto [mat_asset_id, mat] = g_engine->asset_manager()->create_asset(mat_path, MaterialAsset{});
                mat_ids[i] = mat_asset_id;

                mat->material.material_function = R"(
surface.base_color = diffuse;
surface.roughness = roughness;
                )";

                aiColor3D temp_color;
                if (ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, temp_color) == aiReturn_SUCCESS) {
                    mat->material.value_params.emplace_back("diffuse", float3{temp_color.r, temp_color.g, temp_color.b});
                } else {
                    mat->material.value_params.emplace_back("diffuse", float3{0.0f});
                }
                float temp_float;
                if (ai_mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, temp_float) == aiReturn_SUCCESS) {
                    mat->material.value_params.emplace_back("roughness", temp_float);
                } else {
                    mat->material.value_params.emplace_back("roughness", 0.5f);
                }
                mat->material.update_shader_parameter();
            }

            used_names.clear();
            for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
                auto ai_mesh = scene->mMeshes[i];

                auto mesh_name = std::string{ai_mesh->mName.data, ai_mesh->mName.length};
                if (mesh_name.empty() || used_names.contains(mesh_name)) {
                    mesh_name = fmt::format("mesh{}", i);
                }
                used_names.insert(mesh_name);
                if (scene->mNumMeshes > 1) {
                    curr_object = current_scene->create_scene_object(object);
                    curr_object->set_name(mesh_name);
                }

                auto mesh_path = fmt::format("/project/imported/models/{}/{}.static_mesh.biasset", model_name, mesh_name);
                auto [mesh_asset_id, mesh] = g_engine->asset_manager()->create_asset(mesh_path, StaticMesh{});

                mesh->resize(ai_mesh->mNumVertices, ai_mesh->mNumFaces * 3);
                mesh->set_positions_raw(reinterpret_cast<float3 const*>(ai_mesh->mVertices));
                mesh->set_normals_raw(reinterpret_cast<float3 const*>(ai_mesh->mNormals));
                for (uint32_t i = 0; i < ai_mesh->mNumVertices; i++) {
                    mesh->set_texcoord_at(i, {ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y});
                }
                for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++) {
                    auto& ai_face = ai_mesh->mFaces[i];
                    mesh->set_index_at(3 * i, ai_face.mIndices[0]);
                    mesh->set_index_at(3 * i + 1, ai_face.mIndices[1]);
                    mesh->set_index_at(3 * i + 2, ai_face.mIndices[2]);
                }
                mesh->calculate_tspace();
                auto& aabb = ai_mesh->mAABB;
                mesh->set_bouding_box_raw({aabb.mMin.x, aabb.mMin.y, aabb.mMin.z}, {aabb.mMax.x, aabb.mMax.y, aabb.mMax.z});
                mesh->update_gpu_buffer();

                curr_object->attach_component(StaticMeshComponent{
                    .static_mesh = {mesh_asset_id},
                });
                curr_object->attach_component(MeshRendererComponent{
                    .material = {mat_ids[ai_mesh->mMaterialIndex]},
                });
            }

            auto prefab_path = fmt::format("/project/imported/models/{}/{}_prefab.toml", model_name, model_name);
            rt::Prefab::create_from(object, prefab_path);
        }
    );
}

}

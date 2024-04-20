#include "import_model.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/texture.hpp>
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
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>

namespace bi::editor {

auto menu_action_import_model_gltf(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Import Model (glTF)", "Choose File", ".gltf",
        [](std::string choosed_path) {
            std::filesystem::path path{choosed_path};
            if (!std::filesystem::exists(path)) { return; }

            tinygltf::TinyGLTF loader;
            std::string load_err;
            std::string load_warn;
            tinygltf::Model gltf_model;
            if (!loader.LoadASCIIFromFile(&gltf_model, &load_err, &load_warn, path.string())) {
                log::critical("general", "Failed to load mesh file '{}': {}", path.filename().string(), load_err);
                return;
            }
            if (!load_warn.empty()) {
                log::warn("general", "When loading mesh file '{}': {}", path.filename().string(), load_warn);
                return;
            }

            auto model_name = path.filename().string();
            model_name = model_name.substr(0, model_name.find('.'));

            auto current_scene = g_engine->world()->current_scene().value();
            auto base_object = current_scene->create_scene_object();
            base_object->set_name(model_name);

            std::unordered_set<std::string> used_names{};
            std::vector<rt::AssetId> tex_ids{gltf_model.textures.size()};
            std::vector<Ptr<TextureAsset>> tex_assets{gltf_model.textures.size()};
            tinygltf::Image default_gltf_image{};
            default_gltf_image.name = std::string("default");
            default_gltf_image.width = 1;
            default_gltf_image.height = 1;
            default_gltf_image.component = 4;
            default_gltf_image.bits = 8;
            default_gltf_image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            default_gltf_image.image = {255, 255, 255, 255};
            for (size_t i = 0; auto const& gltf_tex : gltf_model.textures) {
                auto tex_name = gltf_tex.name;
                if (tex_name.empty() || used_names.contains(tex_name)) {
                    tex_name = fmt::format("texture{}", i);
                }
                used_names.insert(tex_name);

                auto tex_path = fmt::format("/project/imported/models/{}/{}.texture.biasset", model_name, tex_name);
                auto [tex_asset_id, tex] = g_engine->asset_manager()->create_asset(tex_path, TextureAsset{});
                tex_ids[i] = tex_asset_id;
                tex_assets[i] = tex;

                auto const* gltf_img = &default_gltf_image;
                if (gltf_tex.source >= 0 && gltf_tex.source < gltf_model.images.size()) {
                    gltf_img = &gltf_model.images[gltf_tex.source];
                }
                auto format = rhi::ResourceFormat::rgba8_unorm;

                auto num_pixels = gltf_img->width * gltf_img->height;
                tex->texture_data.resize(gltf_img->width * gltf_img->height * 4);
                for (int i = 0; i < num_pixels; i++) {
                    for (int c = 0; c < gltf_img->component; c++) {
                        tex->texture_data[4 * i + c] = static_cast<std::byte>(gltf_img->image[gltf_img->component * i + c]);
                    }
                    for (int c = gltf_img->component; c < 3; c++) {
                        tex->texture_data[4 * i + c] = tex->texture_data[4 * i];
                    }
                    if (gltf_img->component == 3) {
                        tex->texture_data[4 * i + 3] = static_cast<std::byte>(255);
                    }
                }
                tex->texture = gfx::Texture{
                    gfx::TextureBuilder{}
                        .dim_2d(format, gltf_img->width, gltf_img->height)
                        .mipmap()
                        .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
                };
                tex->update_gpu_data();

                rhi::SamplerDesc sampler_desc{
                    .mag_filter = rhi::SamplerFilterMode::linear,
                    .min_filter = rhi::SamplerFilterMode::linear,
                    .mipmap_mode = rhi::SamplerMipmapMode::linear,
                    .address_mode_u = rhi::SamplerAddressMode::repeat,
                    .address_mode_v = rhi::SamplerAddressMode::repeat,
                    .address_mode_w = rhi::SamplerAddressMode::repeat,
                };
                if (gltf_tex.sampler >= 0 && gltf_tex.sampler < gltf_model.samplers.size()) {
                    auto const& gltf_sampler = gltf_model.samplers[gltf_tex.sampler];
                    if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                        sampler_desc.mag_filter = rhi::SamplerFilterMode::nearest;
                    }
                    if (gltf_sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) {
                        sampler_desc.min_filter = rhi::SamplerFilterMode::nearest;
                    } else if (gltf_sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST) {
                        sampler_desc.min_filter = rhi::SamplerFilterMode::nearest;
                        sampler_desc.mipmap_mode = rhi::SamplerMipmapMode::nearest;
                    } else if (gltf_sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST) {
                        sampler_desc.mipmap_mode = rhi::SamplerMipmapMode::nearest;
                    }
                    if (gltf_sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE) {
                        sampler_desc.address_mode_u = rhi::SamplerAddressMode::clamp_to_edge;
                    } else if (gltf_sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) {
                        sampler_desc.address_mode_u = rhi::SamplerAddressMode::mirror_repeat;
                    }
                    if (gltf_sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE) {
                        sampler_desc.address_mode_v = rhi::SamplerAddressMode::clamp_to_edge;
                    } else if (gltf_sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) {
                        sampler_desc.address_mode_v = rhi::SamplerAddressMode::mirror_repeat;
                    }
                }
                tex->sampler = g_engine->graphics_manager()->get_sampler(sampler_desc);

                ++i;
            }

            used_names.clear();
            std::vector<rt::AssetId> mat_ids{gltf_model.materials.size()};
            for (size_t i = 0; auto const& gltf_mat : gltf_model.materials) {
                auto mat_name = gltf_mat.name;
                if (mat_name.empty() || used_names.contains(mat_name)) {
                    mat_name = fmt::format("material{}", i);
                }
                used_names.insert(mat_name);

                auto mat_path = fmt::format("/project/imported/models/{}/{}.material.toml", model_name, mat_name);
                auto [mat_asset_id, mat] = g_engine->asset_manager()->create_asset(mat_path, MaterialAsset{});
                mat_ids[i] = mat_asset_id;

                auto const& gltf_pbr = gltf_mat.pbrMetallicRoughness;

                auto add_texture = [&](int index, std::string_view tex_name, std::string_view fallback) {
                    auto sampler_name = fmt::format("{}_sampler", tex_name);
                    if (index >= 0) {
                        mat->referenced_textures.emplace_back(tex_name, tex_ids[index]);
                        auto tex = tex_assets[index].value();
                        mat->material.texture_params.emplace_back(tex_name, tex->texture);
                        mat->material.sampler_params.emplace_back(sampler_name, tex->sampler.value());
                    } else {
                        auto [id, tex] = g_engine->asset_manager()->get_and_load_asset<TextureAsset>(
                            fmt::format("/bisemutum/assets/textures/{}.texture.biasset", fallback)
                        );
                        mat->referenced_textures.emplace_back(tex_name, id);
                        mat->material.texture_params.emplace_back(tex_name, tex->texture);
                        mat->material.sampler_params.emplace_back(sampler_name, tex->sampler.value());
                    }
                };

                mat->material.value_params.emplace_back("base_color", float4{
                    gltf_pbr.baseColorFactor[0],
                    gltf_pbr.baseColorFactor[1],
                    gltf_pbr.baseColorFactor[2],
                    gltf_pbr.baseColorFactor[3],
                });
                add_texture(gltf_pbr.baseColorTexture.index, "base_color_tex", "white1x1");

                mat->material.value_params.emplace_back("emission", float3{
                    gltf_mat.emissiveFactor[0],
                    gltf_mat.emissiveFactor[1],
                    gltf_mat.emissiveFactor[2],
                });
                add_texture(gltf_mat.emissiveTexture.index, "emission_tex", "white1x1");

                mat->material.value_params.emplace_back("roughness", static_cast<float>(gltf_pbr.roughnessFactor));
                mat->material.value_params.emplace_back("metallic", static_cast<float>(gltf_pbr.metallicFactor));
                add_texture(gltf_pbr.metallicRoughnessTexture.index, "metallic_roughness_tex", "white1x1");

                mat->material.value_params.emplace_back("normal_map_scale", static_cast<float>(gltf_mat.normalTexture.scale));
                add_texture(gltf_mat.normalTexture.index, "normal_map", "normal1x1");

                mat->material.value_params.emplace_back("occlusion_strength", static_cast<float>(gltf_mat.occlusionTexture.strength));
                add_texture(gltf_mat.occlusionTexture.index, "occlusion_tex", "white1x1");

                // TODO - alpha mode
                mat->material.material_function = R"(
float4 base_color = PARAM_base_color_tex.Sample(PARAM_base_color_tex_sampler, vertex.texcoord) * PARAM_base_color;
surface.base_color = base_color.xyz;
surface.opacity = base_color.w;

float3 normal_map_value = PARAM_normal_map.Sample(PARAM_normal_map_sampler, vertex.texcoord).xyz * 2.0 - 1.0;
normal_map_value = normalize(normal_map_value * float3(PARAM_normal_map_scale, PARAM_normal_map_scale, 1.0));
surface.normal_map_value = normal_map_value * 0.5 + 0.5;

float4 mr_tex = PARAM_metallic_roughness_tex.Sample(PARAM_metallic_roughness_tex_sampler, vertex.texcoord);
surface.roughness = PARAM_roughness * mr_tex.y;
surface.f0_color = lerp(0.04, surface.base_color, PARAM_metallic * mr_tex.z);

float occlusion = PARAM_occlusion_tex.Sample(PARAM_occlusion_tex_sampler, vertex.texcoord) * PARAM_occlusion_strength;
surface.base_color *= occlusion;
surface.f0_color *= occlusion;
surface.f90_color *= occlusion;

float3 emission = PARAM_emission_tex.Sample(PARAM_emission_tex_sampler, vertex.texcoord).xyz * PARAM_emission;
surface.emission = emission;
)";

                mat->material.update_shader_parameter();

                ++i;
            }

            used_names.clear();
            std::vector<rt::AssetId> mesh_ids{gltf_model.meshes.size()};
            for (size_t i = 0; auto const& gltf_mesh : gltf_model.meshes) {
                size_t num_vertices = 0;
                size_t num_indices = 0;
                for (auto const& gltf_prim : gltf_mesh.primitives) {
                    if (gltf_prim.mode != TINYGLTF_MODE_TRIANGLES) { continue; }
                    auto const& pos_acc = gltf_model.accessors[gltf_prim.attributes.at("POSITION")];
                    num_vertices += pos_acc.count;
                    auto const& index_acc = gltf_model.accessors[gltf_prim.indices];
                    num_indices += index_acc.count;
                }
                if (num_vertices == 0) { ++i; continue; }

                auto mesh_name = gltf_mesh.name;
                if (mesh_name.empty() || used_names.contains(mesh_name)) {
                    mesh_name = fmt::format("mesh{}", i);
                }
                used_names.insert(mesh_name);

                auto mesh_path = fmt::format("/project/imported/models/{}/{}.static_mesh.biasset", model_name, mesh_name);
                auto [mesh_asset_id, mesh] = g_engine->asset_manager()->create_asset(mesh_path, StaticMesh{});
                mesh_ids[i] = mesh_asset_id;

                // mesh->resize(num_vertices, num_indices);
                std::vector<gfx::SubmeshDesc> submeshes;
                num_vertices = 0;
                num_indices = 0;
                std::vector<uint32_t> temp_index_buffer_u32;
                std::vector<uint16_t> temp_index_buffer_u16;
                std::vector<uint8_t> temp_index_buffer_u8;
                for (auto const& gltf_prim : gltf_mesh.primitives) {
                    if (gltf_prim.mode != TINYGLTF_MODE_TRIANGLES) {
                        continue;
                    }

                    auto& submesh = submeshes.emplace_back();
                    submesh.base_vertex = num_vertices;
                    submesh.index_offset = num_indices;

                    // indices
                    {
                        auto const& index_acc = gltf_model.accessors[gltf_prim.indices];
                        auto const& buffer_view = gltf_model.bufferViews[index_acc.bufferView];
                        auto const& buffer = gltf_model.buffers[buffer_view.buffer];
                        submesh.num_indices = index_acc.count;
                        num_indices += index_acc.count;
                        switch (index_acc.componentType) {
                            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                                temp_index_buffer_u32.resize(index_acc.count);
                                std::memcpy(
                                    temp_index_buffer_u32.data(),
                                    &buffer.data[index_acc.byteOffset + buffer_view.byteOffset],
                                    index_acc.count * sizeof(uint32_t)
                                );
                                std::copy(
                                    temp_index_buffer_u32.begin(), temp_index_buffer_u32.end(),
                                    std::back_inserter(mesh->get_mutable_mesh_data().mutable_indices())
                                );
                                break;
                            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                                temp_index_buffer_u16.resize(index_acc.count);
                                std::memcpy(
                                    temp_index_buffer_u16.data(),
                                    &buffer.data[index_acc.byteOffset + buffer_view.byteOffset],
                                    index_acc.count * sizeof(uint16_t)
                                );
                                std::copy(
                                    temp_index_buffer_u16.begin(), temp_index_buffer_u16.end(),
                                    std::back_inserter(mesh->get_mutable_mesh_data().mutable_indices())
                                );
                                break;
                            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                                temp_index_buffer_u8.resize(index_acc.count);
                                std::memcpy(
                                    temp_index_buffer_u8.data(),
                                    &buffer.data[index_acc.byteOffset + buffer_view.byteOffset],
                                    index_acc.count * sizeof(uint8_t)
                                );
                                std::copy(
                                    temp_index_buffer_u8.begin(), temp_index_buffer_u8.end(),
                                    std::back_inserter(mesh->get_mutable_mesh_data().mutable_indices())
                                );
                                break;
                            default: unreachable();
                        }
                    }

                    size_t submesh_num_vertices = 0;
                    auto add_vertex_attribute = [&]<typename T>(char const* name, std::vector<T>& vertices) {
                        auto attrib_it = gltf_prim.attributes.find(name);
                        if (attrib_it == gltf_prim.attributes.end()) {
                            vertices.resize(vertices.size() + submesh_num_vertices, T{0.0f});
                            return;
                        }

                        auto const& acc = gltf_model.accessors[attrib_it->second];
                        auto const& buffer_view = gltf_model.bufferViews[acc.bufferView];
                        auto const& buffer = gltf_model.buffers[buffer_view.buffer];
                        auto const* buffer_data = &buffer.data[acc.byteOffset + buffer_view.byteOffset];

                        BI_ASSERT(acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                        if (buffer_view.byteStride == 0) {
                            auto typed_buffer_data = reinterpret_cast<T const*>(buffer_data);
                            std::copy(typed_buffer_data, typed_buffer_data + acc.count, std::back_inserter(vertices));
                        } else {
                            for (size_t i = 0; i < acc.count; i++) {
                                auto& v = vertices.emplace_back();
                                std::memcpy(&v, buffer_data + i * buffer_view.byteStride, sizeof(v));
                            }
                        }
                    };

                    add_vertex_attribute("POSITION", mesh->get_mutable_mesh_data().mutable_positions());
                    submesh_num_vertices = mesh->get_mesh_data().positions().size() - num_vertices;
                    num_vertices += submesh_num_vertices;

                    add_vertex_attribute("NORMAL", mesh->get_mutable_mesh_data().mutable_normals());

                    add_vertex_attribute("TEXCOORD_0", mesh->get_mutable_mesh_data().mutable_texcoords());
                }

                mesh->calculate_tspace();

                ++i;
            }

            used_names.clear();
            size_t num_nodes = 0;
            std::function<auto(Ref<rt::SceneObject>, tinygltf::Node const&) -> void> process_node =
                [&](Ref<rt::SceneObject> parent, tinygltf::Node const& node) {
                    auto node_name = node.name;
                    if (node_name.empty() || used_names.contains(node_name)) {
                        node_name = fmt::format("node{}", num_nodes);
                    }
                    used_names.insert(node_name);
                    ++num_nodes;

                    Transform transform{};
                    if (!node.translation.empty()) {
                        transform.translation = {node.translation[0], node.translation[1], node.translation[2]};
                    }
                    if (!node.rotation.empty()) {
                        transform.set_rotation_with_quaternion({node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]});
                    }
                    if (!node.scale.empty()) {
                        transform.scaling = {node.scale[0], node.scale[1], node.scale[2]};
                    }
                    if (!node.matrix.empty()) {
                        size_t col = 0;
                        size_t row = 0;
                        float4x4 matrix{};
                        for (size_t i = 0; i < 16; i++) {
                            matrix[col][row] = node.matrix[i];
                            if (row == 3) {
                                ++col;
                                row = 0;
                            } else {
                                ++row;
                            }
                        }
                        transform = Transform::from_matrix(matrix);
                    }
                    auto object = current_scene->create_scene_object(parent, transform);
                    object->set_name(node_name);

                    if (node.mesh >= 0) {
                        auto const& gltf_mesh = gltf_model.meshes[node.mesh];
                        size_t prim_ind = 0;
                        std::vector<rt::TAssetPtr<MaterialAsset>> materials;
                        for (auto const& gltf_prim : gltf_mesh.primitives) {
                            if (gltf_prim.mode != TINYGLTF_MODE_TRIANGLES) { continue; }
                            materials.emplace_back(mat_ids[gltf_prim.material]);
                        }
                        object->attach_component(StaticMeshComponent{
                            .static_mesh = {mesh_ids[node.mesh]},
                        });
                        object->attach_component(MeshRendererComponent{
                            .materials = std::move(materials),
                        });
                    }

                    for (int child : node.children) {
                        process_node(object, gltf_model.nodes[child]);
                    }
                };
            for (int node_ind : gltf_model.scenes[std::max(0, gltf_model.defaultScene)].nodes) {
                process_node(base_object, gltf_model.nodes[node_ind]);
            }

            auto prefab_path = fmt::format("/project/imported/models/{}/{}_prefab.toml", model_name, model_name);
            rt::Prefab::create_from(base_object, prefab_path);
        }
    );
}

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
surface.base_color = PARAM_diffuse;
surface.roughness = PARAM_roughness;
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

                curr_object->attach_component(StaticMeshComponent{
                    .static_mesh = {mesh_asset_id},
                });
                curr_object->attach_component(MeshRendererComponent{
                    .materials = {{mat_ids[ai_mesh->mMaterialIndex]}},
                });
            }

            auto prefab_path = fmt::format("/project/imported/models/{}/{}_prefab.toml", model_name, model_name);
            rt::Prefab::create_from(object, prefab_path);
        }
    );
}

}

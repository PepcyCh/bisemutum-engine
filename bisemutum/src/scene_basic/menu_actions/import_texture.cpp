#include "import_texture.hpp"

#include <bisemutum/scene_basic/texture.hpp>
#include <bisemutum/editor/file_dialog.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <tinyexr.h>
#include <stb_image.h>

namespace bi::editor {

auto menu_action_import_texture_hdri(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Import HDRI", "Choose File", ".exr,.hdr",
        [](std::string choosed_path) {
            std::filesystem::path path{choosed_path};
            if (!std::filesystem::exists(path)) { return; }
            rt::PhysicalFile file{path, false};
            auto texture_path = fmt::format("/project/imported/textures/HDRI/{}.texture.biasset", path.stem().string());

            TextureAsset temp_texture;

            if (path.extension().string() == ".exr") {
                float* image_data;
                int width, height;
                const char *err;
                int ret = LoadEXR(&image_data, &width, &height, path.string().c_str(), &err);
                if (ret != 0) { return; }

                for (int i = 0; i < width * height; i++) {
                    auto scale = image_data[4 * i + 3];
                    image_data[4 * i] *= scale;
                    image_data[4 * i + 1] *= scale;
                    image_data[4 * i + 2] *= scale;
                    image_data[4 * i + 3] = 1.0f;
                }

                temp_texture.texture_data.resize(width * height * 4 * sizeof(float));
                std::copy_n(
                    reinterpret_cast<std::byte const*>(image_data), temp_texture.texture_data.size(),
                    temp_texture.texture_data.data()
                );
                temp_texture.texture = gfx::Texture(
                    gfx::TextureBuilder{}
                        .dim_2d(rhi::ResourceFormat::rgba32_sfloat, width, height)
                        .usage(rhi::TextureUsage::sampled)
                );

                free(image_data);
            } else if (path.extension().string() == ".hdr") {
                int width, height, comp;
                auto image_data = stbi_loadf(path.string().c_str(), &width, &height, &comp, 4);
                if (image_data == nullptr) { return; }

                for (int i = 0; i < width * height; i++) {
                    auto scale = image_data[4 * i + 3];
                    image_data[4 * i] *= scale;
                    image_data[4 * i + 1] *= scale;
                    image_data[4 * i + 2] *= scale;
                    image_data[4 * i + 3] = 1.0f;
                }

                temp_texture.texture_data.resize(width * height * 4 * sizeof(float));
                std::copy_n(
                    reinterpret_cast<std::byte const*>(image_data), temp_texture.texture_data.size(),
                    temp_texture.texture_data.data()
                );
                temp_texture.texture = gfx::Texture(
                    gfx::TextureBuilder{}
                        .dim_2d(rhi::ResourceFormat::rgba32_sfloat, width, height)
                        .usage(rhi::TextureUsage::sampled)
                );

                stbi_image_free(image_data);
            } else {
                return;
            }

            temp_texture.update_gpu_data();

            auto cubemap_size = static_cast<uint32_t>(temp_texture.texture.desc().extent.height * 0.6);
            auto [_, texture] = g_engine->asset_manager()->create_asset(texture_path, TextureAsset{});
            texture->texture_data.resize(cubemap_size * cubemap_size * 6 * 4 * sizeof(float));
            texture->texture = gfx::Texture(
                gfx::TextureBuilder{}
                    .dim_cube(rhi::ResourceFormat::rgba32_sfloat, cubemap_size)
                    .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
            );
            texture->sampler = g_engine->graphics_manager()->get_sampler({});
            g_engine->graphics_manager()->execute_in_this_frame(
                [&temp_texture, texture](Ref<rhi::CommandEncoder> cmd) {
                    g_engine->graphics_manager()->equitangular_to_cubemap(cmd, temp_texture.texture, texture->texture);
                }
            );
            texture->update_cpu_data();
        }
    );
}

}

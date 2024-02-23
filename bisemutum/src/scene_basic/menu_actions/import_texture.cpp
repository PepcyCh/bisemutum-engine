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

auto menu_action_import_texture_2d(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Import 2D Texture", "Choose File", ".jpg,.jpeg,.png,.bmp,.tga",
        [](std::string choosed_path) {
            std::filesystem::path path{choosed_path};
            if (!std::filesystem::exists(path)) { return; }
            rt::PhysicalFile file{path, false};
            auto texture_path = fmt::format("/project/imported/textures/2D/{}.texture.biasset", path.stem().string());

            int width, height, temp_comp;
            auto temp_image_data = stbi_load(path.string().c_str(), &width, &height, &temp_comp, 0);
            if (temp_image_data == nullptr) { return; }

            auto num_pixels = width * height;
            auto comp = temp_comp == 3 ? 4 : temp_comp;

            auto [_, texture] = g_engine->asset_manager()->create_asset(texture_path, TextureAsset{});
            texture->texture_data.resize(num_pixels * comp);
            for (int i = 0; i < num_pixels; i++) {
                for (int c = 0; c < temp_comp; c++) {
                    texture->texture_data[comp * i + c] = static_cast<std::byte>(temp_image_data[temp_comp * i + c]);
                }
                if (temp_comp == 3) {
                    texture->texture_data[comp * i + 3] = static_cast<std::byte>(255);
                }
            }

            auto format = comp == 4 ? rhi::ResourceFormat::rgba8_unorm
                : comp == 2 ? rhi::ResourceFormat::rg8_unorm : rhi::ResourceFormat::r8_unorm;
            texture->texture = gfx::Texture(
                gfx::TextureBuilder{}
                    .dim_2d(format, width, height)
                    .mipmap()
                    .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
            );
            texture->sampler = g_engine->graphics_manager()->get_sampler({
                .mag_filter = rhi::SamplerFilterMode::linear,
                .min_filter = rhi::SamplerFilterMode::linear,
                .mipmap_mode = rhi::SamplerMipmapMode::linear,
            });
            texture->update_gpu_data();
        }
    );
}

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

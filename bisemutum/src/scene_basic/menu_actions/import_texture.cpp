#include "import_texture.hpp"

#include <bisemutum/scene_basic/texture.hpp>
#include <bisemutum/editor/file_dialog.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <tinyexr.h>

namespace bi::editor {

auto menu_action_import_texture_hdri(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Import HDRI", "Choose File", ".exr,.hdr",
        [](std::string choosed_path) {
            std::filesystem::path path{choosed_path};
            if (!std::filesystem::exists(path)) { return; }
            rt::PhysicalFile file{path, false};
            auto texture_path = fmt::format("/project/imported/textures/HDRI/{}.texture.biasset", path.stem().string());

            if (path.extension().string() == ".exr") {
                float* image_data;
                int width, height;
                const char *err;
                int ret = LoadEXR(&image_data, &width, &height, path.string().c_str(), &err);
                if (ret != 0) { return; }
                auto [_, texture] = g_engine->asset_manager()->create_asset(texture_path, TextureAsset{});

                for (int i = 0; i < width * height; i++) {
                    auto scale = image_data[4 * i + 3];
                    image_data[4 * i] *= scale;
                    image_data[4 * i + 1] *= scale;
                    image_data[4 * i + 2] *= scale;
                    image_data[4 * i + 3] = 1.0f;
                }

                TextureAsset temp_texture;
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
                temp_texture.update_gpu_data();
                free(image_data);

                auto cubemap_size = static_cast<uint32_t>(height * 0.6);
                texture->texture_data.resize(cubemap_size * cubemap_size * 6 * 4 * sizeof(float));
                texture->texture = gfx::Texture(
                    gfx::TextureBuilder{}
                        .dim_cube(rhi::ResourceFormat::rgba32_sfloat, cubemap_size)
                        .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
                );
                texture->sampler = g_engine->graphics_manager()->get_sampler({});
                g_engine->graphics_manager()->execute_immediately(
                    [&temp_texture, texture](Ref<rhi::CommandEncoder> cmd) {
                        g_engine->graphics_manager()->equitangular_to_cubemap(cmd, temp_texture.texture, texture->texture);
                    }
                );
                texture->update_cpu_data();
            } else if (path.extension().string() == ".hdr") {
                // TODO - support hdr HDRI
            }
        }
    );
}

}

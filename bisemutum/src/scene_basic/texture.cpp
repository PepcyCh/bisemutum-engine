#include <bisemutum/scene_basic/texture.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/rhi/sampler.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace bi {

namespace rhi {

BI_SREFL(
    type(SamplerDesc),
    field(mag_filter),
    field(min_filter),
    field(mipmap_mode),
    field(address_mode_u),
    field(address_mode_v),
    field(address_mode_w),
    field(border_color),
    field(compare_enabled),
    field(compare_op),
    field(anisotropy),
    field(lod_bias),
    field(lod_min),
    field(lod_max)
)

} // namespace rhi

namespace {

struct TextureAssetDesc final {
    std::string image_file;
    rhi::SamplerDesc sampler;
    bool srgb = false;
};
BI_SREFL(
    type(TextureAssetDesc),
    field(image_file),
    field(sampler),
    field(srgb)
)

auto get_format(uint32_t num_bits, uint32_t num_channels, bool srgb, bool is_float) -> rhi::ResourceFormat {
    if (num_bits == 8) {
        if (num_channels == 1) {
            return srgb ? rhi::ResourceFormat::r8_srgb : rhi::ResourceFormat::r8_unorm;
        } else if (num_channels == 2) {
            return srgb ? rhi::ResourceFormat::rg8_srgb : rhi::ResourceFormat::rg8_unorm;
        } else if (num_channels == 4) {
            return srgb ? rhi::ResourceFormat::rgba8_srgb : rhi::ResourceFormat::rgba8_unorm;
        }
    } else if (num_bits == 16) {
        if (num_channels == 1) {
            return is_float ? rhi::ResourceFormat::r16_sfloat : rhi::ResourceFormat::r16_unorm;
        } else if (num_channels == 2) {
            return is_float ? rhi::ResourceFormat::rg16_sfloat : rhi::ResourceFormat::rg16_unorm;
        } else if (num_channels == 4) {
            return is_float ? rhi::ResourceFormat::rgba16_sfloat : rhi::ResourceFormat::rgba16_unorm;
        }
    } else if (num_bits == 32) {
        if (num_channels == 1) {
            return rhi::ResourceFormat::r32_sfloat;
        } else if (num_channels == 2) {
            return rhi::ResourceFormat::rg32_sfloat;
        } else if (num_channels == 4) {
            return rhi::ResourceFormat::rgba32_sfloat;
        }
    }
    return rhi::ResourceFormat::undefined;
}

} // namespace

auto TextureAsset::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    TextureAssetDesc desc{};
    try {
        desc = serde::Value::from_toml(file.read_string_data()).get<TextureAssetDesc>();
    } catch (std::exception const& e) {
        log::critical("general", "Failed to load texture: {}.", e.what());
        return {};
    }

    auto image_file_opt = g_engine->file_system()->get_file(desc.image_file);
    if (!image_file_opt.has_value()) {
        log::critical("general", "Failed to load texture (image file '{}'): File not found.", desc.image_file);
        return {};
    }
    auto& image_file = image_file_opt.value();
    auto image_ext = image_file.extension();
    auto image_bytes = image_file.read_binary_data();

    stbi_set_flip_vertically_on_load(1);
    int num_channels;
    int num_bits;
    bool is_float;
    int width;
    int height;
    stbi_uc* image_data = nullptr;
    std::function<auto(stbi_uc*) -> void> image_deleter = [](stbi_uc* data) { stbi_image_free(data); };
    if (
        image_ext == ".jpg" || image_ext == ".jpeg" || image_ext == ".png" || image_ext == ".bmp"
        || image_ext == ".tga"
    ) {
        num_bits = 8;
        is_float = false;
        image_data = stbi_load_from_memory(
            reinterpret_cast<stbi_uc*>(image_bytes.data()), image_bytes.size(),
            &width, &height, &num_channels, 0
        );
    } else {
        log::critical("general", "Failed to load texture (image file '{}'): Unsupported extension.", image_ext);
        return {};
    }
    if (!image_data) {
        log::critical("general", "Failed to load texture (image file '{}'): Failed to load data.", desc.image_file);
        return {};
    }

    if (num_channels == 3) {
        num_channels = 4;
        auto num_pixels = width * height;
        auto pixel_size = num_bits / 8;
        auto new_image_data = new stbi_uc[num_pixels * pixel_size * num_channels];
        image_deleter = [](stbi_uc* data) { delete[] data; };

        std::function<auto(stbi_uc*) -> void> fill_alpha;
        if (num_bits == 8) {
            fill_alpha = [](stbi_uc* data) { *data = 1; };
        } else if (num_bits == 16) {
            if (is_float) {
                fill_alpha = [](stbi_uc* data) { *reinterpret_cast<uint16_t*>(data) = 0x3c00; };
            } else {
                fill_alpha = [](stbi_uc* data) { *reinterpret_cast<uint16_t*>(data) = 1; };
            }
        } else if (num_bits == 32) {
            fill_alpha = [](stbi_uc* data) { *reinterpret_cast<float*>(data) = 1.0f; };
        }

        for (uint32_t i = 0; i < num_pixels; i++) {
            std::copy_n(image_data + i * 3 * pixel_size, 3 * pixel_size, new_image_data + i * 4 * pixel_size);
            fill_alpha(new_image_data + (i * 4 + 3) * pixel_size);
        }

        std::swap(new_image_data, image_data);
        stbi_image_free(new_image_data);
    }

    auto format = get_format(num_bits, num_channels, desc.srgb, is_float);
    if (format == rhi::ResourceFormat::undefined) {
        image_deleter(image_data);
        log::critical("general", "Failed to load texture (image file '{}'): Failed to detect format.", desc.image_file);
        return {};
    }

    rhi::TextureDesc texture_desc = gfx::TextureBuilder()
        .dim_2d(format, width, height)
        .mipmap()
        .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::color_attachment});
    TextureAsset texture{
        .texture = texture_desc,
        .sampler = g_engine->graphics_manager()->get_sampler(desc.sampler),
    };

    auto image_pitch = width * (num_bits / 8) * num_channels;
    gfx::Buffer temp_buffer{gfx::BufferBuilder().size(image_pitch * height).mem_upload()};
    g_engine->graphics_manager()->execute_immediately(
        [&temp_buffer, &texture, image_pitch, width, height](Ref<rhi::CommandEncoder> cmd) {
            auto access = BitFlags{rhi::ResourceAccessType::transfer_write};
            cmd->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = texture.texture.rhi_texture(),
                    .dst_access_type = access,
                },
            });
            cmd->copy_buffer_to_texture(
                temp_buffer.rhi_buffer(),
                texture.texture.rhi_texture(),
                rhi::BufferTextureCopyDesc{
                    .buffer_pixels_per_row = static_cast<uint32_t>(width),
                    .buffer_rows_per_texture = static_cast<uint32_t>(height),
                }
            );
            g_engine->graphics_manager()->generate_mipmaps_2d(cmd, texture.texture, access);
        }
    );

    return texture;
}

}

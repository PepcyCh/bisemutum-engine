#include <bisemutum/scene_basic/texture.hpp>

#include <bisemutum/prelude/byte_stream.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/rhi/sampler.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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
    auto binary_data = file.read_binary_data();
    ReadByteStream bs{binary_data};

    uint32_t magic_number = 0;
    std::string asset_type_name;
    uint32_t version = 0;
    bs.read(magic_number).read(asset_type_name).read(version);
    if (!rt::check_if_binary_asset_valid<TextureAsset>(file.filename(), magic_number, asset_type_name)) {
        return {};
    }

    rhi::SamplerDesc sampler_desc{};
    bs.read(sampler_desc);
    rhi::TextureDesc texture_desc{};
    bs.read(texture_desc);

    TextureAsset texture{
        .texture = texture_desc,
        .sampler = g_engine->graphics_manager()->get_sampler(sampler_desc),
    };

    if (version == 1) {
        uint32_t storage_type = 0;
        bs.read(storage_type);
        if (storage_type == 0) {
            bs.read(texture.texture_data);
        } else if (storage_type == 1) {
            auto bytes_per_layer =
                texture_desc.extent.width * texture_desc.extent.height * rhi::format_texel_size(texture_desc.format);
            texture.texture_data.resize(bytes_per_layer * texture_desc.extent.depth_or_layers);
            for (uint32_t layer = 0; layer < texture_desc.extent.depth_or_layers; layer++) {
                std::vector<std::byte> png_data;
                bs.read(png_data);
                int temp_width, temp_height, temp_comp;
                auto image_data = stbi_load_from_memory(
                    reinterpret_cast<stbi_uc*>(png_data.data()), png_data.size(),
                    &temp_width, &temp_height, &temp_comp, 0
                );
                std::copy_n(
                    reinterpret_cast<std::byte const*>(image_data), bytes_per_layer,
                    texture.texture_data.data() + layer * bytes_per_layer
                );
            }
        }
    } else {
        ReadByteStream data_bs{};
        bs.read_compressed_part(data_bs);
        data_bs.read(texture.texture_data);
    }

    texture.update_gpu_data();

    return texture;
}

auto TextureAsset::save(Dyn<rt::IFile>::Ref file) const -> void {
    WriteByteStream bs{};
    bs.write(rt::asset_magic_number).write(TextureAsset::asset_type_name).write(2u);

    auto& sampler_desc = sampler->rhi_sampler()->desc();
    bs.write(sampler_desc);
    auto& texture_desc = texture.desc();
    bs.write(texture_desc);

    auto data_from = bs.curr_offset();
    bs.write(texture_data);
    bs.compress_data(data_from);

    file.write_binary_data(bs.data());
}

auto TextureAsset::update_gpu_data() -> void {
    gfx::Buffer temp_buffer{gfx::BufferBuilder().size(texture_data.size()).mem_upload()};
    temp_buffer.set_data_raw(texture_data.data(), texture_data.size());
    g_engine->graphics_manager()->execute_in_this_frame(
        [this, &temp_buffer](Ref<rhi::CommandEncoder> cmd) {
            auto access = BitFlags{rhi::ResourceAccessType::transfer_write};
            cmd->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = texture.rhi_texture(),
                    .dst_access_type = access,
                },
            });
            auto& extent = texture.desc().extent;
            cmd->copy_buffer_to_texture(
                temp_buffer.rhi_buffer(),
                texture.rhi_texture(),
                rhi::BufferTextureCopyDesc{
                    .buffer_pixels_per_row = extent.width,
                    .buffer_rows_per_texture = extent.height,
                }
            );
            if (texture.desc().levels > 1) {
                g_engine->graphics_manager()->generate_mipmaps_2d(cmd, texture, access);
                BI_ASSERT(access == rhi::ResourceAccessType::sampled_texture_read);
            } else {
                cmd->resource_barriers({}, {
                    rhi::TextureBarrier{
                        .texture = texture.rhi_texture(),
                        .src_access_type = access,
                        .dst_access_type = rhi::ResourceAccessType::sampled_texture_read,
                    },
                });
            }
        }
    );
}

auto TextureAsset::update_cpu_data() -> void {
    gfx::Buffer temp_buffer{gfx::BufferBuilder().size(texture_data.size()).mem_readback()};
    g_engine->graphics_manager()->execute_in_this_frame(
        [this, &temp_buffer](Ref<rhi::CommandEncoder> cmd) {
            auto access = BitFlags{rhi::ResourceAccessType::transfer_read};
            cmd->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = texture.rhi_texture(),
                    .dst_access_type = access,
                },
            });
            auto& extent = texture.desc().extent;
            cmd->copy_texture_to_buffer(
                texture.rhi_texture(),
                temp_buffer.rhi_buffer(),
                rhi::BufferTextureCopyDesc{
                    .buffer_pixels_per_row = extent.width,
                    .buffer_rows_per_texture = extent.height,
                }
            );
            cmd->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = texture.rhi_texture(),
                    .src_access_type = access,
                    .dst_access_type = rhi::ResourceAccessType::sampled_texture_read,
                },
            });
        }
    );
    temp_buffer.get_data_raw(texture_data.data(), texture_data.size());
}

}

#include <iostream>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/scene_basic/texture.hpp>

auto do_create_texture_asset(int argc, char** argv) -> void {
    std::filesystem::path output_path{"./texture.texture.biasset"};
    if (argc > 1) {
        output_path = argv[1];
    }

    bi::rt::PhysicalFile file(output_path, true);

    bi::TextureAsset texture{};
    texture.sampler = bi::g_engine->graphics_manager()->get_sampler(bi::rhi::SamplerDesc{
        .mag_filter = bi::rhi::SamplerFilterMode::linear,
        .min_filter = bi::rhi::SamplerFilterMode::linear,
        .address_mode_u = bi::rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = bi::rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = bi::rhi::SamplerAddressMode::clamp_to_edge,
    });

    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    texture.texture_data = {
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
    };

    texture.texture = bi::rhi::TextureDesc{
        .extent = {width, height, depth},
        .levels = 1,
        .format = bi::rhi::ResourceFormat::rgba8_unorm,
        .dim = bi::rhi::TextureDimension::d2,
        .usages = {bi::rhi::TextureUsage::sampled},
    };

    texture.save(file);
}

int main(int argc, char** argv) {
    auto dummy_project_path = std::string{"./tools/dummy_project/project.toml"};
    std::array<char*, 2> dummy_args{
        argv[0],
        dummy_project_path.data(),
    };
    if (!bi::initialize_engine(dummy_args.size(), dummy_args.data())) { return -1; }

    do_create_texture_asset(argc, argv);

    if (!bi::finalize_engine()) { return -2; }
    return 0;
}

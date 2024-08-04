#include "rasterization_order.hpp"

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(InitializePassParams)
    BI_SHADER_PARAMETER_UAV_BUFFER(RWStructuredBuffer<uint>, counting)
BI_SHADER_PARAMETERS_END()

struct InitializePassData final {
    gfx::BufferHandle counting;
};

BI_SHADER_PARAMETERS_BEGIN(ShowOrderPassParams)
    BI_SHADER_PARAMETER_UAV_BUFFER(RWStructuredBuffer<uint>, counting)
BI_SHADER_PARAMETERS_END()

struct ShowOrderPassData final {
    gfx::BufferHandle counting;
    gfx::TextureHandle output;
};

}

RasterizationOrderPass::RasterizationOrderPass() {
    initialize_shader_params_.initialize<InitializePassParams>();
    initialize_shader_.source.path = "/bisemutum/shaders/renderer/rasterization_order/initialize.hlsl";
    initialize_shader_.source.entry = "ro_initialize_cs";
    initialize_shader_.set_shader_params_struct<InitializePassParams>();

    fragment_shader_params_.initialize<ShowOrderPassParams>();
    fragment_shader_.source.path = "/bisemutum/shaders/renderer/rasterization_order/show_order.hlsl";
    fragment_shader_.source.entry = "ro_show_order_fs";
    fragment_shader_.set_shader_params_struct<ShowOrderPassParams>();
    fragment_shader_.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    fragment_shader_.depth_test = false;
}

auto RasterizationOrderPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
    auto& camera_target = camera.target_texture();

    auto counting_buffer = rg.add_buffer([](gfx::BufferBuilder& builder) {
        builder.size(sizeof(uint32_t)).usage({rhi::BufferUsage::storage_read_write});
    });

    {
        auto [builder, pass_data] = rg.add_compute_pass<InitializePassData>("Rasterization Order Initialize");

        pass_data->counting = builder.write(counting_buffer);
        counting_buffer = pass_data->counting;

        builder.set_execution_function<InitializePassData>(
            [this, counting_buffer](CRef<InitializePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = initialize_shader_params_.mutable_typed_data<InitializePassParams>();
                params->counting = {ctx.rg->buffer(counting_buffer)};
                initialize_shader_params_.update_uniform_buffer();
                ctx.dispatch(initialize_shader_, initialize_shader_params_);
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_graphics_pass<ShowOrderPassData>("Rasterization Order Show");

        pass_data->counting = builder.write(counting_buffer);
        counting_buffer = pass_data->counting;

        pass_data->output = rg.import_back_buffer();
        builder.use_color(0, pass_data->output);

        builder.set_execution_function<ShowOrderPassData>(
            [this, counting_buffer, &camera](CRef<ShowOrderPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                auto params = fragment_shader_params_.mutable_typed_data<ShowOrderPassParams>();
                params->counting = {ctx.rg->buffer(counting_buffer)};
                fragment_shader_params_.update_uniform_buffer();
                ctx.render_full_screen(camera, fragment_shader_, fragment_shader_params_);
            }
        );
    }
}

}

#include <bisemutum/graphics/render_graph_context.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/prelude/math.hpp>

#include "descriptor_sets.hpp"

namespace bi::gfx {

ResourceBindingContext::ResourceBindingContext() : temp_set_samplers(possible_max_sets) {}

auto ResourceBindingContext::set_shader_params(
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set, BitFlags<rhi::ShaderStage> visibility,
    ShaderParameter& params
) -> void {
    auto& metadata_list = params.metadata_list();

    std::vector<rhi::DescriptorHandle> cpu_descriptors{};
    // Only `.type`, `.count`, '.binding_or_register', 'visibility' are needed
    rhi::BindGroupLayout layout{};
    if (params.uniform_buffer().has_value()) {
        cpu_descriptors.push_back(params.uniform_buffer().get_cbv());
        layout.push_back(rhi::BindGroupLayoutEntry{
            .count = 1,
            .type = rhi::DescriptorType::uniform_buffer,
            .visibility = visibility,
            .binding_or_register = 0,
        });
    }

    size_t cpu_size = 0;
    uint32_t curr_binding = 1;
    auto separate_samplers = g_engine->graphics_manager()->device()->properties().separate_sampler_heap;
    if (separate_samplers) {
        temp_set_samplers[set].cpu_descriptors.clear();
        temp_set_samplers[set].layout.clear();
    }

    for (auto& param : metadata_list.params) {
        uint32_t count = 1;
        for (auto sz : param.array_sizes) { count *= sz; }

        switch (param.descriptor_type) {
            case rhi::DescriptorType::uniform_buffer:
            case rhi::DescriptorType::read_only_storage_buffer:
            case rhi::DescriptorType::read_write_storage_buffer:
                for (uint32_t i = 0; i < count; i++) {
                    cpu_size = aligned_size(cpu_size, param.cpu_alignment);
                    auto buffer = params.typed_data_offset<BufferParam>(cpu_size);
                    cpu_descriptors.push_back(buffer->buffer->get_descriptor(
                        param, buffer->offset, buffer->size
                    ));
                    cpu_size += param.size;
                }
                break;
            case rhi::DescriptorType::sampled_texture:
                for (uint32_t i = 0; i < count; i++) {
                    cpu_size = aligned_size(cpu_size, param.cpu_alignment);
                    auto texture = params.typed_data_offset<TextureParam>(cpu_size);
                    cpu_descriptors.push_back(texture->texture->get_descriptor(
                        param,
                        texture->base_level, texture->num_levels,
                        texture->base_layer, texture->num_layers
                    ));
                    cpu_size += param.size;
                }
                break;
            case rhi::DescriptorType::read_write_storage_texture:
                for (uint32_t i = 0; i < count; i++) {
                    cpu_size = aligned_size(cpu_size, param.cpu_alignment);
                    auto texture = params.typed_data_offset<RWTextureParam>(cpu_size);
                    cpu_descriptors.push_back(texture->texture->get_descriptor(
                        param,
                        texture->mip_level, 1,
                        texture->base_layer, texture->num_layers
                    ));
                    cpu_size += param.size;
                }
                break;
            case rhi::DescriptorType::sampler:
                for (uint32_t i = 0; i < count; i++) {
                    cpu_size = aligned_size(cpu_size, param.cpu_alignment);
                    auto sampler = params.typed_data_offset<SamplerState>(cpu_size)->sampler.value();
                    if (separate_samplers) {
                        temp_set_samplers[set].cpu_descriptors.push_back(sampler->get_descriptor());
                    } else {
                        cpu_descriptors.push_back(sampler->get_descriptor());
                    }
                    cpu_size += param.size;
                }
                break;
            default:
                for (uint32_t i = 0; i < count; i++) {
                    cpu_size = aligned_size(cpu_size, param.cpu_alignment) + param.size;
                }
                break;
        }
        if (param.descriptor_type == rhi::DescriptorType::none) {
            continue;
        }

        rhi::BindGroupLayoutEntry layout_entry{
            .count = count,
            .type = param.descriptor_type,
            .visibility = visibility,
            .binding_or_register = curr_binding,
        };
        if (separate_samplers && param.descriptor_type == rhi::DescriptorType::sampler) {
            layout_entry.binding_or_register += set * samplers_binding_shift;
            temp_set_samplers[set].layout.push_back(std::move(layout_entry));
            temp_set_samplers[set].layout.back().visibility = graphics_set_visibility_samplers;
        } else {
            layout.push_back(std::move(layout_entry));
        }
        curr_binding += count;
    }

    auto descriptor = g_engine->graphics_manager()->get_gpu_descriptor_for(std::move(cpu_descriptors), layout);
    cmd_encoder->set_descriptors(set, {descriptor});
}

auto ResourceBindingContext::set_samplers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set) -> void {
    uint32_t num_samplers = 0;
    uint32_t num_entries = 0;
    for (auto const& set_samplers : temp_set_samplers) {
        num_samplers += set_samplers.cpu_descriptors.size();
        num_entries += set_samplers.layout.size();
    }
    if (num_samplers == 0) { return; }

    std::vector<rhi::DescriptorHandle> cpu_descriptors(num_samplers);
    rhi::BindGroupLayout layout(num_entries);
    uint32_t p_samplers = 0;
    uint32_t p_entries = 0;
    for (auto const& set_samplers : temp_set_samplers) {
        std::copy(
            set_samplers.cpu_descriptors.begin(), set_samplers.cpu_descriptors.end(),
            cpu_descriptors.begin() + p_samplers
        );
        p_samplers += set_samplers.cpu_descriptors.size();
        std::copy(
            set_samplers.layout.begin(), set_samplers.layout.end(),
            layout.begin() + p_entries
        );
        p_entries += set_samplers.layout.size();
    }

    auto descriptor = g_engine->graphics_manager()->get_gpu_descriptor_for(std::move(cpu_descriptors), layout);
    cmd_encoder->set_descriptors(set, {descriptor});
}


GraphicsPassContext::GraphicsPassContext(
    CRef<RenderGraph> rg,
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder,
    std::vector<rhi::ResourceFormat>&& color_targets_format,
    rhi::ResourceFormat depth_stencil_format
)
    : rg(rg)
    , cmd_encoder(cmd_encoder)
    , color_targets_format(std::move(color_targets_format))
    , depth_stencil_format(depth_stencil_format)
{
    resource_binding_ctx_ = Box<ResourceBindingContext>::make();
}

auto GraphicsPassContext::render_list(RenderedObjectListHandle handle, ShaderParameter& params) const -> void {
    auto list = rg->rendered_object_list(handle);
    for (auto& item : list->items) {
        auto pipeline = g_engine->graphics_manager()->compile_pipeline_for_drawable(
            this, list->camera, item.drawables[0], list->fragmeng_shader
        );
        cmd_encoder->set_pipeline(pipeline);
        resource_binding_ctx_->set_shader_params(
            cmd_encoder, graphics_set_camera, graphics_set_visibility_camera,
            list->camera.remove_const()->shader_params()
        );
        resource_binding_ctx_->set_shader_params(
            cmd_encoder, graphics_set_fragment, graphics_set_visibility_fragment, params
        );
        for (auto drawable : item.drawables) {
            drawable->mesh->fill_shader_params(drawable);
            resource_binding_ctx_->set_shader_params(
                cmd_encoder, graphics_set_mesh, graphics_set_visibility_mesh, drawable->shader_params
            );
            resource_binding_ctx_->set_shader_params(
                cmd_encoder, graphics_set_material, graphics_set_visibility_material,
                drawable->material->shader_parameters
            );
            resource_binding_ctx_->set_samplers(cmd_encoder, graphics_set_samplers);

            drawable->mesh->bind_buffers(cmd_encoder);
            cmd_encoder->draw_indexed(drawable->mesh->num_indices());
        }
    }
}

}

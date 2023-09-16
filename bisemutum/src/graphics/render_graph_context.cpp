#include <bisemutum/graphics/render_graph_context.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/prelude/math.hpp>

#include "descriptor_sets.hpp"

namespace bi::gfx {

ResourceBindingContext::ResourceBindingContext() : temp_set_samplers(possible_max_sets) {}

auto ResourceBindingContext::set_shader_params(
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set, ShaderParameter& params
) -> void {
    params.update_uniform_buffer();
    auto& metadata_list = params.metadata_list();

    std::vector<rhi::DescriptorHandle> cpu_descriptors{};
    std::vector<rhi::DescriptorType> desc_types{};
    // Only `.type`, `.count`, '.binding_or_register' are needed
    rhi::BindGroupLayout layout{};
    if (params.uniform_buffer().has_value()) {
        cpu_descriptors.push_back(params.uniform_buffer().get_cbv());
        desc_types.push_back(rhi::DescriptorType::uniform_buffer);
        layout.push_back(rhi::BindGroupLayoutEntry{
            .count = 1,
            .type = rhi::DescriptorType::uniform_buffer,
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
                    desc_types.push_back(param.descriptor_type);
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
                    desc_types.push_back(param.descriptor_type);
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
                    desc_types.push_back(param.descriptor_type);
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
                        desc_types.push_back(param.descriptor_type);
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

        rhi::BindGroupLayoutEntry layout_entry{
            .count = count,
            .type = param.descriptor_type,
            .binding_or_register = curr_binding,
        };
        if (separate_samplers && param.descriptor_type == rhi::DescriptorType::sampler) {
            layout_entry.binding_or_register += set * samplers_binding_shift;
            temp_set_samplers[set].layout.push_back(std::move(layout_entry));
        } else {
            layout.push_back(std::move(layout_entry));
        }
        curr_binding += count;
    }

    auto descriptor = g_engine->graphics_manager()->get_descriptors_for(std::move(cpu_descriptors), desc_types, layout);
    cmd_encoder->set_descriptors(set, {descriptor});
}

auto ResourceBindingContext::set_samplers(Ref<rhi::GraphicsCommandEncoder> cmd_encoder, uint32_t set) -> void {
    uint32_t num_samplers = 0;
    uint32_t num_entries = 0;
    for (auto const& set_samplers : temp_set_samplers) {
        num_samplers += set_samplers.cpu_descriptors.size();
        num_entries += set_samplers.layout.size();
    }

    std::vector<rhi::DescriptorHandle> cpu_descriptors(num_samplers);
    std::vector<rhi::DescriptorType> desc_types(num_samplers, rhi::DescriptorType::sampler);
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

    auto descriptor = g_engine->graphics_manager()->get_descriptors_for(std::move(cpu_descriptors), desc_types, layout);
    cmd_encoder->set_descriptors(set, {descriptor});
}


GraphicsPassContext::GraphicsPassContext(
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder,
    std::vector<rhi::ResourceFormat>&& color_targets_format,
    rhi::ResourceFormat depth_stencil_format
)
    : cmd_encoder(cmd_encoder)
    , color_targets_format(std::move(color_targets_format))
    , depth_stencil_format(depth_stencil_format)
{
    resource_binding_ctx_ = Box<ResourceBindingContext>::make();
}

auto GraphicsPassContext::get_rendered_object_list(RenderedObjectListDesc const& desc) const -> RenderedObjectList {
    std::vector<Ref<Drawable>> drawables;
    g_engine->graphics_manager()->for_each_drawable([this, &drawables, &desc](Drawable& drawable) {
        auto mat_is_opaque = drawable.material->blend_mode == BlendMode::opaque
            || drawable.material->blend_mode == BlendMode::alpha_test;
        if (
            (desc.type.contains(RenderedObjectType::opaque) && mat_is_opaque)
            || (desc.type.contains(RenderedObjectType::transparent) && !mat_is_opaque)
        ) {
            drawables.push_back(drawable);
        }
    });
    std::sort(drawables.begin(), drawables.end(), [](Ref<Drawable> a, Ref<Drawable> b) {
        if (is_poly_ptr_address_same<IMesh>(a->mesh, b->mesh)) {
            return a->material->base_material() < b->material->base_material();
        } else {
            return a->mesh.raw() < b->mesh.raw();
        }
    });

    RenderedObjectList list{
        .camera = desc.camera,
    };
    void* curr_mesh;
    Ptr<Material> curr_mat;
    for (size_t i = 0, j = 0; j < drawables.size(); j++) {
        curr_mesh = drawables[j]->mesh.raw();
        curr_mat = drawables[j]->material->base_material();

        if (
            j + 1 == drawables.size()
            || curr_mesh != drawables[j + 1]->mesh.raw()
            || curr_mat != drawables[j + 1]->material->base_material()
        ) {
            auto pipeline = g_engine->graphics_manager()->compile_pipeline_for_drawable(
                this, desc.camera, drawables[i], desc.fragmeng_shader
            );
            RenderedObjectListItem item{pipeline};
            item.drawables.reserve(j - i);
            for (auto k = i; k < j; k++) {
                item.drawables.push_back(drawables[k]);
            }
            list.items.push_back(std::move(item));
            i = j;
        }
    }

    return list;
}

auto GraphicsPassContext::render_list(RenderedObjectList& list, ShaderParameter& params) const -> void {
    for (auto& item : list.items) {
        cmd_encoder->set_pipeline(item.pipeline);
        resource_binding_ctx_->set_shader_params(
            cmd_encoder, graphics_set_camera, list.camera.remove_const()->shader_params()
        );
        resource_binding_ctx_->set_shader_params(
            cmd_encoder, graphics_set_fragment, params
        );
        for (auto drawable : item.drawables) {
            drawable->mesh->fill_shader_params(drawable);
            resource_binding_ctx_->set_shader_params(
                cmd_encoder, graphics_set_mesh, drawable->shader_params
            );
            resource_binding_ctx_->set_shader_params(
                cmd_encoder, graphics_set_material, drawable->material->shader_parameters
            );
            resource_binding_ctx_->set_samplers(cmd_encoder, graphics_set_samplers);

            drawable->mesh->bind_buffers(cmd_encoder);
            cmd_encoder->draw_indexed(drawable->mesh->num_indices());
        }
    }
}

}

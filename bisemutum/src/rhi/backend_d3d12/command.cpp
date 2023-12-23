#include "command.hpp"

#ifndef NDEBUG
#define USE_PIX
#include <WinPixEventRuntime/pix3.h>
#endif

#include "device.hpp"
#include "resource.hpp"
#include "pipeline.hpp"
#include "utils.hpp"

namespace bi::rhi {

namespace {

#ifndef NDEBUG
auto encode_event_color(float r, float g, float b) -> UINT64 {
    return PIX_COLOR(static_cast<BYTE>(r * 255.0f), static_cast<BYTE>(g * 255.0f), static_cast<BYTE>(b * 255.0f));
}
#endif

auto push_label_impl(ID3D12GraphicsCommandList4* cmd_list, CommandLabel const& label) -> void {
#ifndef NDEBUG
    auto color = encode_event_color(label.color.r, label.color.g, label.color.b);
    auto owned_label = std::string{label.label};
    PIXBeginEvent(cmd_list, color, owned_label.c_str());
#endif
}

auto pop_label_impl(ID3D12GraphicsCommandList4* cmd_list) -> void {
#ifndef NDEBUG
    PIXEndEvent(cmd_list);
#endif
}

auto to_dx_buffer_state(BitFlags<ResourceAccessType> type) -> D3D12_RESOURCE_STATES {
    auto states = D3D12_RESOURCE_STATE_COMMON;
    if (
        type.contains_any(ResourceAccessType::vertex_buffer_read)
        || type.contains_any(ResourceAccessType::uniform_buffer_read)
    ) {
        states |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if (type.contains_any(ResourceAccessType::index_buffer_read)) {
        states |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }
    if (type.contains_any(ResourceAccessType::indirect_read)) {
        states |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    if (type.contains_any(ResourceAccessType::storage_resource_read)) {
        states |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
    if (type.contains_any(ResourceAccessType::depth_stencil_attachment_write)) {
        states |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (type.contains_any(ResourceAccessType::transfer_read)) {
        states |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (type.contains_any(ResourceAccessType::transfer_write)) {
        states |= D3D12_RESOURCE_STATE_COPY_DEST;
    }
    return states;
}

auto to_dx_texture_state(BitFlags<ResourceAccessType> type) -> D3D12_RESOURCE_STATES {
    auto states = D3D12_RESOURCE_STATE_COMMON;
    if (
        type.contains_any(ResourceAccessType::sampled_texture_read)
        || type.contains_any(ResourceAccessType::storage_resource_read)
    ) {
        states |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
    if (type.contains_any(ResourceAccessType::storage_resource_write)) {
        states |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (
        type.contains_any(ResourceAccessType::color_attachment_read)
        || type.contains_any(ResourceAccessType::color_attachment_write)
    ) {
        states |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if (type.contains_any(ResourceAccessType::depth_stencil_attachment_read)) {
        states |= D3D12_RESOURCE_STATE_DEPTH_READ;
    }
    if (type.contains_any(ResourceAccessType::depth_stencil_attachment_write)) {
        states |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if (type.contains_any(ResourceAccessType::transfer_read)) {
        states |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (type.contains_any(ResourceAccessType::transfer_write)) {
        states |= D3D12_RESOURCE_STATE_COPY_DEST;
    }
    if (type.contains_any(ResourceAccessType::resolve_read)) {
        states |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    }
    if (type.contains_any(ResourceAccessType::resolve_write)) {
        states |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
    }
    if (type.contains_any(ResourceAccessType::present)) {
        states |= D3D12_RESOURCE_STATE_PRESENT;
    }
    return states;
}

} // namespace

CommandPoolD3D12::CommandPoolD3D12(Ref<DeviceD3D12> device, CommandPoolDesc const& desc) : device_(device) {
    device_->raw()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_allocator_));
}

auto CommandPoolD3D12::reset() -> void {
    cmd_allocator_->Reset();
    available_cmd_list_index_ = 0;
}

auto CommandPoolD3D12::get_command_encoder() -> Box<CommandEncoder> {
    ID3D12GraphicsCommandList4* cmd_list = nullptr;
    if (available_cmd_list_index_ < allocated_cmd_lists_.size()) {
        cmd_list = allocated_cmd_lists_[available_cmd_list_index_++].Get();
        cmd_list->Reset(cmd_allocator_.Get(), nullptr);
    } else {
        allocated_cmd_lists_.emplace_back();
        device_->raw()->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocator_.Get(), nullptr,
            IID_PPV_ARGS(&allocated_cmd_lists_.back())
        );
        cmd_list = allocated_cmd_lists_.back().Get();
        ++available_cmd_list_index_;
    }

    return Box<CommandEncoderD3D12>::make(device_, cmd_list);
}


CommandBufferD3D12::CommandBufferD3D12(Ref<DeviceD3D12> device, ID3D12GraphicsCommandList4* cmd_list)
    : device_(device), cmd_list_(cmd_list) {}


CommandEncoderD3D12::CommandEncoderD3D12(Ref<DeviceD3D12> device, ID3D12GraphicsCommandList4* cmd_list)
    : device_(device), cmd_list_(cmd_list) {}

CommandEncoderD3D12::~CommandEncoderD3D12() {
    assert(cmd_list_ == nullptr);
}

auto CommandEncoderD3D12::finish() -> Box<CommandBuffer> {
    cmd_list_->Close();
    auto cmd_buffer = Box<CommandBufferD3D12>::make(device_, cmd_list_);
    cmd_list_ = nullptr;
    return cmd_buffer;
}

auto CommandEncoderD3D12::push_label(CommandLabel const& label) -> void {
    push_label_impl(cmd_list_, label);
}

auto CommandEncoderD3D12::pop_label() -> void {
    pop_label_impl(cmd_list_);
}

auto CommandEncoderD3D12::copy_buffer_to_buffer(
    CRef<Buffer> src_buffer, Ref<Buffer> dst_buffer, BufferCopyDesc const& region
) -> void {
    auto src_buffer_dx = src_buffer.cast_to<const BufferD3D12>();
    auto dst_buffer_dx = dst_buffer.cast_to<BufferD3D12>();
    auto length = std::min({
        region.length, src_buffer_dx->desc().size - region.src_offset, dst_buffer_dx->desc().size - region.dst_offset,
    });
    cmd_list_->CopyBufferRegion(
        dst_buffer_dx->raw(), region.dst_offset, src_buffer_dx->raw(), region.src_offset, length
    );
}

auto CommandEncoderD3D12::copy_texture_to_texture(
        CRef<Texture> src_texture, Ref<Texture> dst_texture, TextureCopyDesc const& region
) -> void {
    auto src_texture_dx = src_texture.cast_to<const TextureD3D12>();
    auto dst_texture_dx = dst_texture.cast_to<TextureD3D12>();
    D3D12_TEXTURE_COPY_LOCATION src_loc{
        .pResource = src_texture_dx->raw(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = src_texture_dx->subresource_index(region.src_level, region.src_layer),
    };
    D3D12_TEXTURE_COPY_LOCATION dst_loc{
        .pResource = dst_texture_dx->raw(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = dst_texture_dx->subresource_index(region.dst_level, region.dst_layer),
    };
    D3D12_BOX copy_box{
        .left = region.src_offset.x,
        .top = region.src_offset.y,
        .front = region.src_offset.z,
        .right = region.src_offset.x + region.extent.width,
        .bottom = region.src_offset.y + region.extent.height,
        .back = region.src_offset.z + region.extent.depth_or_layers,
    };
    cmd_list_->CopyTextureRegion(
        &dst_loc, region.dst_offset.x, region.dst_offset.y, region.dst_offset.z, &src_loc, &copy_box
    );
}

auto CommandEncoderD3D12::copy_buffer_to_texture(
    CRef<Buffer> src_buffer, Ref<Texture> dst_texture, BufferTextureCopyDesc const& region
) -> void {
    auto src_buffer_dx = src_buffer.cast_to<const BufferD3D12>();
    auto dst_texture_dx = dst_texture.cast_to<TextureD3D12>();
    auto& extent = dst_texture_dx->desc().extent;
    uint32_t tex_depth, tex_layers;
    dst_texture_dx->get_depth_and_layer(extent.depth_or_layers, tex_depth, tex_layers);
    uint32_t region_depth, region_layers;
    dst_texture_dx->get_depth_and_layer(region.texture_extent.depth_or_layers, region_depth, region_layers);
    region_layers = std::min(region_layers, tex_layers - region.texture_layer);
    auto row_pitch = region.buffer_pixels_per_row * format_texel_size(dst_texture_dx->desc().format);
    auto layer_size = region.buffer_rows_per_texture * row_pitch;
    for (uint32_t layer = 0; layer < region_layers; layer++) {
        D3D12_TEXTURE_COPY_LOCATION src_loc{
            .pResource = src_buffer_dx->raw(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{
                .Offset = region.buffer_offset + layer * layer_size,
                .Footprint = {
                    .Format = to_dx_format(dst_texture_dx->desc().format),
                    .Width = std::min(region.texture_extent.width, extent.width),
                    .Height = std::min(region.texture_extent.height, extent.height),
                    .Depth = std::min(region_depth, tex_depth),
                    .RowPitch = row_pitch,
                }
            },
        };
        D3D12_TEXTURE_COPY_LOCATION dst_loc{
            .pResource = dst_texture_dx->raw(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = dst_texture_dx->subresource_index(region.texture_level, region.texture_layer + layer),
        };
        // auto temp_texture_desc = dst_texture_dx->raw()->GetDesc();
        // temp_texture_desc.Width = region.texture_extent.width;
        // temp_texture_desc.Height = region.texture_extent.height;
        // temp_texture_desc.DepthOrArraySize = region.texture_extent.depth_or_layers;
        // temp_texture_desc.MipLevels = 1;
        // device_->raw()->GetCopyableFootprints(
        //     &temp_texture_desc, 0, 1, region.buffer_offset, &src_loc.PlacedFootprint, nullptr, nullptr, nullptr
        // );
        D3D12_BOX copy_box{
            .left = 0,
            .top = 0,
            .front = 0,
            .right = std::min(region.texture_extent.width, extent.width),
            .bottom = std::min(region.texture_extent.height, extent.height),
            .back = std::min(region_depth, tex_depth),
        };
        cmd_list_->CopyTextureRegion(
            &dst_loc, region.texture_offset.x, region.texture_offset.y, region.texture_offset.z, &src_loc, &copy_box
        );
    }
}

auto CommandEncoderD3D12::copy_texture_to_buffer(
    CRef<Texture> src_texture, Ref<Buffer> dst_buffer, BufferTextureCopyDesc const& region
) -> void {
    auto src_texture_dx = src_texture.cast_to<const TextureD3D12>();
    auto dst_buffer_dx = dst_buffer.cast_to<BufferD3D12>();
    auto& extent = src_texture_dx->desc().extent;
    uint32_t tex_depth, tex_layers;
    src_texture_dx->get_depth_and_layer(extent.depth_or_layers, tex_depth, tex_layers);
    uint32_t region_depth, region_layers;
    src_texture_dx->get_depth_and_layer(region.texture_extent.depth_or_layers, region_depth, region_layers);
    region_layers = std::min(region_layers, tex_layers - region.texture_layer);
    auto row_pitch = region.buffer_pixels_per_row * format_texel_size(src_texture_dx->desc().format);
    auto layer_size = region.buffer_rows_per_texture * row_pitch;
    for (uint32_t layer = 0; layer < region_layers; layer++) {
        D3D12_TEXTURE_COPY_LOCATION src_loc{
            .pResource = src_texture_dx->raw(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = src_texture_dx->subresource_index(region.texture_level, region.texture_layer + layer),
        };
        D3D12_TEXTURE_COPY_LOCATION dst_loc{
            .pResource = dst_buffer_dx->raw(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT{
                .Offset = region.buffer_offset + layer_size,
                .Footprint = {
                    .Format = to_dx_format(src_texture_dx->desc().format),
                    .Width = std::min(region.texture_extent.width, extent.width),
                    .Height = std::min(region.texture_extent.height, extent.height),
                    .Depth = std::min(region_depth, tex_depth),
                    .RowPitch = row_pitch,
                }
            },
        };
        // auto temp_texture_desc = src_texture_dx->raw()->GetDesc();
        // temp_texture_desc.Width = region.texture_extent.width;
        // temp_texture_desc.Height = region.texture_extent.height;
        // temp_texture_desc.DepthOrArraySize = region.texture_extent.depth_or_layers;
        // temp_texture_desc.MipLevels = 1;
        // device_->raw()->GetCopyableFootprints(
        //     &temp_texture_desc, 0, 1, region.buffer_offset, &dst_loc.PlacedFootprint, nullptr, nullptr, nullptr
        // );
        D3D12_BOX copy_box{
            .left = region.texture_offset.x,
            .top = region.texture_offset.y,
            .front = region.texture_offset.z,
            .right = std::min(region.texture_offset.x + region.texture_extent.width, extent.width),
            .bottom = std::min(region.texture_offset.y + region.texture_extent.height, extent.height),
            .back = std::min(region.texture_offset.z + region_depth, tex_depth),
        };
        cmd_list_->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, &copy_box);
    }
}

auto CommandEncoderD3D12::resource_barriers(
    CSpan<BufferBarrier> buffer_barriers, CSpan<TextureBarrier> texture_barriers
) -> void {
    std::vector<D3D12_RESOURCE_BARRIER> barriers_dx{};
    barriers_dx.reserve(buffer_barriers.size() + texture_barriers.size());
    for (auto const& barrier : buffer_barriers) {
        auto buffer_dx = barrier.buffer.cast_to<BufferD3D12>();
        if (buffer_dx->is_state_restricted()) {
            continue;
        }
        auto src_states = to_dx_buffer_state(barrier.src_access_type);
        if (barrier.src_access_type == ResourceAccessType::none) {
            src_states = buffer_dx->get_current_state();
        }
        auto dst_states = to_dx_buffer_state(barrier.dst_access_type);
        buffer_dx->set_current_state(dst_states);
        if (src_states != dst_states) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = buffer_dx->raw(),
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = src_states,
                    .StateAfter = dst_states,
                },
            });
        } else if ((src_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV = D3D12_RESOURCE_UAV_BARRIER{
                    .pResource = buffer_dx->raw(),
                },
            });
        }
    }
    for (auto const& barrier : texture_barriers) {
        auto texture_dx = barrier.texture.cast_to<TextureD3D12>();
        auto src_states = to_dx_texture_state(barrier.src_access_type);
        if (barrier.src_access_type == ResourceAccessType::none) {
            src_states = texture_dx->get_current_state();
        }
        auto dst_states = to_dx_texture_state(barrier.dst_access_type);
        texture_dx->set_current_state(dst_states);
        uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        if (barrier.num_levels == 1 || barrier.base_layer == 1) {
            subresource = texture_dx->subresource_index(barrier.base_level, barrier.base_layer);
        }
        if (src_states != dst_states) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
                    .pResource = texture_dx->raw(),
                    .Subresource = subresource,
                    .StateBefore = src_states,
                    .StateAfter = dst_states,
                },
            });
        } else if ((src_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV = D3D12_RESOURCE_UAV_BARRIER{
                    .pResource = texture_dx->raw(),
                },
            });
        }
    }

    if (!barriers_dx.empty()) {
        cmd_list_->ResourceBarrier(barriers_dx.size(), barriers_dx.data());
    }
}

auto CommandEncoderD3D12::set_descriptor_heaps(CSpan<Ref<DescriptorHeap>> heaps) -> void {
    std::vector<ID3D12DescriptorHeap*> heaps_dx(heaps.size());
    for (size_t i = 0; i < heaps.size(); i++) {
        heaps_dx[i] = heaps[i].cast_to<DescriptorHeapD3D12>()->raw();
    }
    cmd_list_->SetDescriptorHeaps(heaps_dx.size(), heaps_dx.data());
}

auto CommandEncoderD3D12::begin_render_pass(
    CommandLabel const& label, RenderTargetDesc const& desc
) -> Box<GraphicsCommandEncoder> {
    if (!label.label.empty()) {
        push_label(label);
    }
    auto render_encoder = Box<GraphicsCommandEncoderD3D12>::make(
        device_, desc, unsafe_make_ref(this), !label.label.empty()
    );
    cmd_list_ = nullptr;
    return render_encoder;
}

auto CommandEncoderD3D12::begin_compute_pass(CommandLabel const& label) -> Box<ComputeCommandEncoder> {
    if (!label.label.empty()) {
        push_label(label);
    }
    auto compute_encoder = Box<ComputeCommandEncoderD3D12>::make(
        device_, unsafe_make_ref(this), !label.label.empty()
    );
    cmd_list_ = nullptr;
    return compute_encoder;
}


GraphicsCommandEncoderD3D12::GraphicsCommandEncoderD3D12(
    Ref<DeviceD3D12> device,
    RenderTargetDesc const& desc,
    Ref<CommandEncoderD3D12> base_encoder,
    bool has_label
)
    : device_(device), base_encoder_(base_encoder), has_label_(has_label)
{
    cmd_list_ = base_encoder->cmd_list_;

    std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> colors_desc(desc.colors.size());
    for (size_t i = 0; i < desc.colors.size(); i++) {
        auto const& target = desc.colors[i];
        auto texture_dx = desc.colors[i].texture.texture.cast_to<TextureD3D12>();
        auto color_format = to_dx_format(texture_dx->desc().format);
        colors_desc[i] = D3D12_RENDER_PASS_RENDER_TARGET_DESC{
            .cpuDescriptor = {
                texture_dx->get_render_target_view(
                    target.texture.mip_level, target.texture.base_layer, target.texture.num_layers,
                    color_format, TextureViewType::automatic
                )
            },
            .BeginningAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS{
                .Type = desc.colors[i].clear
                    ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR
                    : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
                .Clear = D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS{
                    .ClearValue = D3D12_CLEAR_VALUE{
                        .Format = color_format,
                        .Color = {
                            target.clear_color.r, target.clear_color.g, target.clear_color.b, target.clear_color.a,
                        }
                    }
                }
            },
            .EndingAccess = D3D12_RENDER_PASS_ENDING_ACCESS{
                .Type = desc.colors[i].store
                    ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                    : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
                .Resolve = {}, // TODO - support resolve
            }
        };
    }

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_desc{};
    bool has_depth_stencil = desc.depth_stencil.has_value();
    if (has_depth_stencil) {
        auto const& depth_stencil = desc.depth_stencil.value();
        auto texture_dx = depth_stencil.texture.texture.cast_to<TextureD3D12>();
        auto depth_stencil_format = to_dx_format(texture_dx->desc().format);
        depth_stencil_desc = D3D12_RENDER_PASS_DEPTH_STENCIL_DESC{
            .cpuDescriptor = {
                texture_dx->get_render_target_view(
                    depth_stencil.texture.mip_level, depth_stencil.texture.base_layer, depth_stencil.texture.num_layers,
                    depth_stencil_format, TextureViewType::automatic
                )
            },
            .DepthBeginningAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS{
                .Type = depth_stencil.clear
                    ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR
                    : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
                .Clear = D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS{
                    .ClearValue = D3D12_CLEAR_VALUE{
                        .Format = depth_stencil_format,
                        .DepthStencil = D3D12_DEPTH_STENCIL_VALUE{
                            .Depth = depth_stencil.clear_depth,
                        }
                    }
                }
            },
            .StencilBeginningAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS{
                .Type = depth_stencil.clear 
                    ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR
                    : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
                .Clear = D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS{
                    .ClearValue = D3D12_CLEAR_VALUE{
                        .Format = depth_stencil_format,
                        .DepthStencil = D3D12_DEPTH_STENCIL_VALUE{
                            .Stencil = static_cast<UINT8>(depth_stencil.clear_stencil),
                        }
                    }
                }
            },
            .DepthEndingAccess = D3D12_RENDER_PASS_ENDING_ACCESS {
                .Type = depth_stencil.store
                    ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                    : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
                .Resolve = {}, // TODO - support resolve
            },
            .StencilEndingAccess = D3D12_RENDER_PASS_ENDING_ACCESS {
                .Type = depth_stencil.store
                    ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                    : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
                .Resolve = {},
            }
        };
    }

    cmd_list_->BeginRenderPass(
        colors_desc.size(), colors_desc.data(),
        has_depth_stencil ? &depth_stencil_desc : nullptr,
        D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES
    );
}

GraphicsCommandEncoderD3D12::~GraphicsCommandEncoderD3D12() {
    cmd_list_->EndRenderPass();

    if (has_label_) {
        pop_label();
    }
    base_encoder_->cmd_list_ = cmd_list_;
}

auto GraphicsCommandEncoderD3D12::push_label(CommandLabel const& label) -> void {
    push_label_impl(cmd_list_, label);
}

auto GraphicsCommandEncoderD3D12::pop_label() -> void {
    pop_label_impl(cmd_list_);
}

auto GraphicsCommandEncoderD3D12::set_pipeline(CRef<GraphicsPipeline> pipeline) -> void {
    curr_pipeline_ = pipeline.cast_to<const GraphicsPipelineD3D12>().get();

    cmd_list_->SetPipelineState(curr_pipeline_->raw());
    cmd_list_->SetGraphicsRootSignature(curr_pipeline_->raw_root_signature());
    cmd_list_->IASetPrimitiveTopology(curr_pipeline_->primitive_topology());
}

auto GraphicsCommandEncoderD3D12::set_descriptors(
    uint32_t from_group_index, CSpan<DescriptorHandle> descriptors
) -> void {
    for (size_t i = 0; i < descriptors.size(); i++) {
        cmd_list_->SetGraphicsRootDescriptorTable(from_group_index + i, {descriptors[i].gpu});
    }
}

auto GraphicsCommandEncoderD3D12::push_constants(void const* data, uint32_t size, uint32_t offset) -> void {
    cmd_list_->SetGraphicsRoot32BitConstants(
        curr_pipeline_->root_constant_index(),
        size / 4, data, offset / 4
    );
}

auto GraphicsCommandEncoderD3D12::set_viewports(CSpan<Viewport> viewports) -> void {
    std::vector<D3D12_VIEWPORT> viewports_dx(viewports.size());
    for (size_t i = 0; i < viewports_dx.size(); i++) {
        viewports_dx[i] = D3D12_VIEWPORT{
            .TopLeftX = viewports[i].x,
            .TopLeftY = viewports[i].y,
            .Width = viewports[i].width,
            .Height = viewports[i].height,
            .MinDepth = viewports[i].min_depth,
            .MaxDepth = viewports[i].max_depth,
        };
    }
    cmd_list_->RSSetViewports(viewports_dx.size(), viewports_dx.data());
}

auto GraphicsCommandEncoderD3D12::set_scissors(CSpan<Scissor> scissors) -> void {
    std::vector<D3D12_RECT> scissors_dx(scissors.size());
    for (size_t i = 0; i < scissors_dx.size(); i++) {
        scissors_dx[i] = D3D12_RECT{
            .left = static_cast<LONG>(scissors[i].x),
            .top = static_cast<LONG>(scissors[i].y),
            .right = static_cast<LONG>(scissors[i].x + scissors[i].width),
            .bottom = static_cast<LONG>(scissors[i].y + scissors[i].height),
        };
    }
    cmd_list_->RSSetScissorRects(scissors_dx.size(), scissors_dx.data());
}

auto GraphicsCommandEncoderD3D12::set_vertex_buffer(
    CSpan<Ref<Buffer>> buffers, CSpan<uint64_t> offsets, uint32_t first_binding
) -> void {
    std::vector<D3D12_VERTEX_BUFFER_VIEW> views(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
        auto buffer_dx = buffers[i].cast_to<BufferD3D12>();
        auto offset = i < offsets.size() ? offsets[i] : 0;
        views[i] = D3D12_VERTEX_BUFFER_VIEW{
            .BufferLocation = buffer_dx->raw()->GetGPUVirtualAddress() + offset,
            .SizeInBytes = static_cast<uint32_t>(buffer_dx->size() - offset),
            .StrideInBytes = curr_pipeline_->desc().vertex_input_buffers[first_binding + i].stride
        };
    }
    cmd_list_->IASetVertexBuffers(first_binding, views.size(), views.data());
}

auto GraphicsCommandEncoderD3D12::set_index_buffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) -> void {
    auto buffer_dx = buffer.cast_to<BufferD3D12>();
    D3D12_INDEX_BUFFER_VIEW view{
        .BufferLocation = buffer_dx->raw()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = static_cast<uint32_t>(buffer_dx->size() - offset),
        .Format = to_dx_index_format(index_type),
    };
    cmd_list_->IASetIndexBuffer(&view);
}

auto GraphicsCommandEncoderD3D12::draw(
    uint32_t num_vertices, uint32_t num_instance, uint32_t first_vertex, uint32_t first_instance
) -> void {
    cmd_list_->DrawInstanced(num_vertices, num_instance, first_vertex, first_instance);
}

auto GraphicsCommandEncoderD3D12::draw_indexed(
    uint32_t num_indices, uint32_t num_instance, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance
) -> void {
    cmd_list_->DrawIndexedInstanced(num_indices, num_instance, first_index, vertex_offset, first_instance);
}


ComputeCommandEncoderD3D12::ComputeCommandEncoderD3D12(
    Ref<DeviceD3D12> device, Ref<CommandEncoderD3D12> base_encoder, bool has_label
) : device_(device), base_encoder_(base_encoder), has_label_(has_label) {
    cmd_list_ = base_encoder->cmd_list_;
}

ComputeCommandEncoderD3D12::~ComputeCommandEncoderD3D12() {
    if (has_label_) {
        pop_label();
    }
    base_encoder_->cmd_list_ = cmd_list_;
}

auto ComputeCommandEncoderD3D12::push_label(CommandLabel const& label) -> void {
    push_label_impl(cmd_list_, label);
}

auto ComputeCommandEncoderD3D12::pop_label() -> void {
    pop_label_impl(cmd_list_);
}

auto ComputeCommandEncoderD3D12::set_pipeline(CRef<ComputePipeline> pipeline) -> void {
    curr_pipeline_ = pipeline.cast_to<const ComputePipelineD3D12>().get();
    cmd_list_->SetPipelineState(curr_pipeline_->raw());
    cmd_list_->SetComputeRootSignature(curr_pipeline_->raw_root_signature());
}

auto ComputeCommandEncoderD3D12::set_descriptors(
    uint32_t from_group_index, CSpan<DescriptorHandle> descriptors
) -> void {
    for (size_t i = 0; i < descriptors.size(); i++) {
        cmd_list_->SetComputeRootDescriptorTable(from_group_index + i, {descriptors[i].gpu});
    }
}

auto ComputeCommandEncoderD3D12::push_constants(void const* data, uint32_t size, uint32_t offset) -> void {
    cmd_list_->SetComputeRoot32BitConstants(
        curr_pipeline_->root_constant_index(),
        size / 4, data, offset / 4
    );
}

auto ComputeCommandEncoderD3D12::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) -> void {
    cmd_list_->Dispatch(num_groups_x, num_groups_y, num_groups_z);
}

}

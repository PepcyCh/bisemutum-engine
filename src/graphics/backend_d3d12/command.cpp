#include "command.hpp"

#define USE_PIX
#include <WinPixEventRuntime/pix3.h>

#include "device.hpp"
#include "resource.hpp"
#include "pipeline.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

UINT64 EncodeEventColor(float r, float g, float b) {
    return PIX_COLOR(static_cast<BYTE>(r * 255.0f), static_cast<BYTE>(g * 255.0f), static_cast<BYTE>(b * 255.0f));
}

D3D12_RESOURCE_STATES ToDxBufferState(BitFlags<ResourceAccessType> type, BitFlags<ResourceAccessStage> stage) {
    D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
    if (type.Contains(ResourceAccessType::eVertexBufferRead) || type.Contains(ResourceAccessType::eUniformBufferRead)) {
        states |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if (type.Contains(ResourceAccessType::eIndexBufferRead)) {
        states |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }
    if (type.Contains(ResourceAccessType::eIndirectRead)) {
        states |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    if (type.Contains(ResourceAccessType::eStorageResourceRead)) {
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            states |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            states |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
    }
    if (type.Contains(ResourceAccessType::eStorageResourceWrite)) {
        states |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (type.Contains(ResourceAccessType::eTransferRead)) {
        states |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (type.Contains(ResourceAccessType::eTransferWrite)) {
        states |= D3D12_RESOURCE_STATE_COPY_DEST;
    }
    return states;
}

D3D12_RESOURCE_STATES ToDxTextureState(BitFlags<ResourceAccessType> type, BitFlags<ResourceAccessStage> stage) {
    D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
    if (type.Contains(ResourceAccessType::eSampledTextureRead)
        || type.Contains(ResourceAccessType::eStorageResourceRead)) {
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            states |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            states |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
    }
    if (type.Contains(ResourceAccessType::eStorageResourceWrite)) {
        states |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (type.Contains(ResourceAccessType::eRenderAttachmentRead)) {
        if (stage.Contains(ResourceAccessStage::eColorAttachment)) {
            states |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        if (stage.Contains(ResourceAccessStage::eDepthStencilAttachment)) {
            states |= D3D12_RESOURCE_STATE_DEPTH_READ;
        }
    }
    if (type.Contains(ResourceAccessType::eRenderAttachmentWrite)) {
        if (stage.Contains(ResourceAccessStage::eColorAttachment)) {
            states |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        if (stage.Contains(ResourceAccessStage::eDepthStencilAttachment)) {
            states |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
    }
    if (type.Contains(ResourceAccessType::eTransferRead)) {
        if (stage.Contains(ResourceAccessStage::eTransfer)) {
            states |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        }
        if (stage.Contains(ResourceAccessStage::eResolve)) {
            states |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        }
    }
    if (type.Contains(ResourceAccessType::eTransferWrite)) {
        if (stage.Contains(ResourceAccessStage::eTransfer)) {
            states |= D3D12_RESOURCE_STATE_COPY_DEST;
        }
        if (stage.Contains(ResourceAccessStage::eResolve)) {
            states |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
        }
    }
    if (type.Contains(ResourceAccessType::ePresent)) {
        states |= D3D12_RESOURCE_STATE_PRESENT;
    }
    return states;
}

}

CommandBufferD3D12::CommandBufferD3D12(Ref<DeviceD3D12> device, ID3D12GraphicsCommandList4 *cmd_list)
    : device_(device), cmd_list_(cmd_list) {}


CommandEncoderD3D12::CommandEncoderD3D12(Ref<DeviceD3D12> device, Ref<FrameContextD3D12> context,
    ID3D12GraphicsCommandList4 *cmd_list) : device_(device), context_(context), cmd_list_(cmd_list) {}

CommandEncoderD3D12::~CommandEncoderD3D12() {
    BI_ASSERT(cmd_list_ == nullptr);
}

Ptr<CommandBuffer> CommandEncoderD3D12::Finish() {
    cmd_list_->Close();
    auto cmd_buffer = Ptr<CommandBufferD3D12>::Make(device_, cmd_list_);
    cmd_list_ = nullptr;
    return cmd_buffer;
}

void CommandEncoderD3D12::PushLabel(const CommandLabel &label) {
    UINT64 color = EncodeEventColor(label.color.r, label.color.g, label.color.b);
    PIXBeginEvent(cmd_list_, color, label.label.c_str());
}

void CommandEncoderD3D12::PopLabel() {
    PIXEndEvent(cmd_list_);
}

void CommandEncoderD3D12::CopyBufferToBuffer(Ref<Buffer> src_buffer, Ref<Buffer> dst_buffer, Span<BufferCopyDesc> regions) {
    auto src_buffer_dx = src_buffer.CastTo<BufferD3D12>();
    auto dst_buffer_dx = dst_buffer.CastTo<BufferD3D12>();
    for (const auto &region : regions) {
        UINT64 length = std::min({ region.length, src_buffer_dx->Size() - region.src_offset,
            dst_buffer_dx->Size() - region.dst_offset });
        cmd_list_->CopyBufferRegion(dst_buffer_dx->Raw(), region.dst_offset, src_buffer_dx->Raw(), region.src_offset, length);
    }
}

void CommandEncoderD3D12::CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture, 
    Span<TextureCopyDesc> regions) {
    auto src_texture_dx = src_texture.CastTo<TextureD3D12>();
    auto dst_texture_dx = dst_texture.CastTo<TextureD3D12>();
    for (const auto &region : regions) {
        uint32_t src_base_depth, src_base_layer;
        src_texture_dx->GetDepthAndLayer(region.src_offset.z, src_base_depth, src_base_layer);
        uint32_t dst_base_depth, dst_base_layer;
        src_texture_dx->GetDepthAndLayer(region.dst_offset.z, dst_base_depth, dst_base_layer);
        uint32_t depth, layers;
        src_texture_dx->GetDepthAndLayer(region.extent.depth_or_layers, depth, layers);
        layers = std::min({ layers, src_texture_dx->Layers() - src_base_layer,
            dst_texture_dx->Layers() - dst_base_layer });
        for (uint32_t array = 0; array < layers; array++) {
            D3D12_TEXTURE_COPY_LOCATION src_loc {
                .pResource = src_texture_dx->Raw(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = src_texture_dx->SubresourceIndex(region.src_level, array + src_base_layer),
            };
            D3D12_TEXTURE_COPY_LOCATION dst_loc {
                .pResource = dst_texture_dx->Raw(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = dst_texture_dx->SubresourceIndex(region.dst_level, array + dst_base_layer),
            };
            D3D12_BOX copy_box {
                .left = region.src_offset.x,
                .top = region.src_offset.y,
                .front = src_base_depth,
                .right = region.src_offset.x + region.extent.width,
                .bottom = region.src_offset.y + region.extent.height,
                .back = src_base_depth + depth,
            };
            cmd_list_->CopyTextureRegion(&dst_loc, region.dst_offset.x, region.dst_offset.y, dst_base_depth,
                &src_loc, &copy_box);
        }
    }
}

void CommandEncoderD3D12::CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture, 
    Span<BufferTextureCopyDesc> regions) {
    auto src_buffer_dx = src_buffer.CastTo<BufferD3D12>();
    auto dst_texture_dx = dst_texture.CastTo<TextureD3D12>();
    for (const auto &region : regions) {
        uint32_t dst_base_depth, dst_base_layer;
        dst_texture_dx->GetDepthAndLayer(region.texture_offset.z, dst_base_depth, dst_base_layer);
        uint32_t depth, layers;
        dst_texture_dx->GetDepthAndLayer(region.texture_extent.depth_or_layers, depth, layers);
        layers = std::min(layers, dst_texture_dx->Layers() - dst_base_layer);
        for (uint32_t array = 0; array < layers; array++) {
            D3D12_TEXTURE_COPY_LOCATION src_loc {
                .pResource = src_buffer_dx->Raw(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
                    .Offset = region.buffer_offset,
                    .Footprint = {
                        .Format = ToDxFormat(dst_texture_dx->Desc().format),
                        .Width = region.texture_extent.width,
                        .Height = region.texture_extent.height,
                        .Depth = depth,
                        .RowPitch = region.buffer_bytes_per_row,
                    }
                },
            };
            D3D12_TEXTURE_COPY_LOCATION dst_loc {
                .pResource = dst_texture_dx->Raw(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = dst_texture_dx->SubresourceIndex(region.texture_level, array + dst_base_layer),
            };
            D3D12_BOX copy_box {
                .left = 0,
                .top = 0,
                .front = 0,
                .right = region.texture_extent.width,
                .bottom = region.texture_extent.height,
                .back = depth,
            };
            cmd_list_->CopyTextureRegion(&dst_loc, region.texture_offset.x, region.texture_offset.y, dst_base_depth,
                &src_loc, &copy_box);
        }
    }
}

void CommandEncoderD3D12::CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer, 
    Span<BufferTextureCopyDesc> regions) {
    auto src_texture_dx = src_texture.CastTo<TextureD3D12>();
    auto dst_buffer_dx = dst_buffer.CastTo<BufferD3D12>();
    for (const auto &region : regions) {
        uint32_t src_base_depth, src_base_layer;
        src_texture_dx->GetDepthAndLayer(region.texture_offset.z, src_base_depth, src_base_layer);
        uint32_t depth, layers;
        src_texture_dx->GetDepthAndLayer(region.texture_extent.depth_or_layers, depth, layers);
        layers = std::min(layers, src_texture_dx->Layers() - src_base_layer);
        for (uint32_t array = 0; array < layers; array++) {
            D3D12_TEXTURE_COPY_LOCATION src_loc {
                .pResource = src_texture_dx->Raw(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = src_texture_dx->SubresourceIndex(region.texture_level, array + src_base_layer),
            };
            D3D12_TEXTURE_COPY_LOCATION dst_loc {
                .pResource = dst_buffer_dx->Raw(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT {
                    .Offset = region.buffer_offset,
                    .Footprint = {
                        .Format = ToDxFormat(src_texture_dx->Desc().format),
                        .Width = region.texture_extent.width,
                        .Height = region.texture_extent.height,
                        .Depth = depth,
                        .RowPitch = region.buffer_bytes_per_row,
                    }
                },
            };
            D3D12_BOX copy_box {
                .left = region.texture_offset.x,
                .top = region.texture_offset.y,
                .front = src_base_depth,
                .right = region.texture_offset.x + region.texture_extent.width,
                .bottom = region.texture_offset.y + region.texture_extent.height,
                .back = src_base_depth + depth,
            };
            cmd_list_->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, &copy_box);
        }
    }
}

void CommandEncoderD3D12::ResourceBarrier(Span<BufferBarrier> buffer_barriers, Span<TextureBarrier> texture_barriers) {
    Vec<D3D12_RESOURCE_BARRIER> barriers_dx;
    barriers_dx.reserve(buffer_barriers.Size() + texture_barriers.Size());
    for (const auto &barrier : buffer_barriers) {
        auto buffer_dx = barrier.buffer.CastTo<BufferD3D12>();
        if (buffer_dx->IsStateRestricted()) {
            continue;
        }
        auto src_states = ToDxBufferState(barrier.src_access_type, barrier.src_access_stage);
        auto dst_states = ToDxBufferState(barrier.dst_access_type, barrier.dst_access_stage);
        if (src_states != dst_states) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
                    .pResource = buffer_dx->Raw(),
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = src_states,
                    .StateAfter = dst_states,
                },
            });
        } else if ((src_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV = D3D12_RESOURCE_UAV_BARRIER {
                    .pResource = buffer_dx->Raw(),
                },
            });
        }
    }
    for (const auto &barrier : texture_barriers) {
        const auto texture_dx = barrier.texture.texture.CastTo<TextureD3D12>();
        if (texture_dx->IsStateRestricted()) {
            continue;
        }
        auto src_states = ToDxTextureState(barrier.src_access_type, barrier.src_access_stage);
        auto dst_states = ToDxTextureState(barrier.dst_access_type, barrier.dst_access_stage);
        if (src_states != dst_states) {
            if (barrier.texture.base_layer == 0 && barrier.texture.layers >= texture_dx->Layers()
                && barrier.texture.base_level == 0 && barrier.texture.levels >= texture_dx->Desc().levels) {
                barriers_dx.push_back(D3D12_RESOURCE_BARRIER {
                    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                    .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                    .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
                        .pResource = texture_dx->Raw(),
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = src_states,
                        .StateAfter = dst_states,
                    },
                });
            } else {
                for (uint32_t layer = 0; layer < barrier.texture.layers; layer++) {
                    if (barrier.texture.base_layer + layer >= texture_dx->Layers()) {
                        break;
                    }
                    for (uint32_t level = 0; level < barrier.texture.levels; level++) {
                        if (barrier.texture.base_level + level >= texture_dx->Desc().levels) {
                            break;
                        }
                        barriers_dx.push_back(D3D12_RESOURCE_BARRIER {
                            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                            .Transition = D3D12_RESOURCE_TRANSITION_BARRIER {
                                .pResource = texture_dx->Raw(),
                                .Subresource = texture_dx->SubresourceIndex(barrier.texture.base_level + level,
                                    barrier.texture.base_layer + layer),
                                .StateBefore = src_states,
                                .StateAfter = dst_states,
                            },
                        });
                    }
                }
            }
        } else if ((src_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0) {
            barriers_dx.push_back(D3D12_RESOURCE_BARRIER {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV = D3D12_RESOURCE_UAV_BARRIER {
                    .pResource = texture_dx->Raw(),
                },
            });
        }
    }

    if (!barriers_dx.empty()) {
        cmd_list_->ResourceBarrier(barriers_dx.size(), barriers_dx.data());
    }
}

Ptr<RenderCommandEncoder> CommandEncoderD3D12::BeginRenderPass(const CommandLabel &label, const RenderTargetDesc &desc) {
    if (!label.label.empty()) {
        PushLabel(label);
    }
    auto render_encoder = Ptr<RenderCommandEncoderD3D12>::Make(device_, desc, RefThis(), label.label);
    cmd_list_ = nullptr;
    return render_encoder;
}

Ptr<ComputeCommandEncoder> CommandEncoderD3D12::BeginComputePass(const CommandLabel &label) {
    if (!label.label.empty()) {
        PushLabel(label);
    }
    auto compute_encoder = Ptr<ComputeCommandEncoderD3D12>::Make(device_, RefThis(), label.label);
    cmd_list_ = nullptr;
    return compute_encoder;
}


RenderCommandEncoderD3D12::RenderCommandEncoderD3D12(Ref<DeviceD3D12> device, const RenderTargetDesc &desc,
    Ref<CommandEncoderD3D12> base_encoder, const std::string &label)
    : device_(device), base_encoder_(base_encoder), label_(label) {
    cmd_list_ = base_encoder->cmd_list_;

    Vec<D3D12_RENDER_PASS_RENDER_TARGET_DESC> colors_desc(desc.colors.size());
    for (size_t i = 0; i < desc.colors.size(); i++) {
        auto texture_dx = desc.colors[i].texture.texture.CastTo<TextureD3D12>();
        colors_desc[i] = D3D12_RENDER_PASS_RENDER_TARGET_DESC {
            .cpuDescriptor = texture_dx->GetView(TextureRenderTargetViewD3D12Desc {
                .base_layer = desc.colors[i].texture.base_layer,
                .layers = desc.colors[i].texture.layers,
                .base_level = desc.colors[i].texture.base_level,
                .levels = desc.colors[i].texture.levels,
            }).cpu,
            .BeginningAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS {
                .Type = desc.colors[i].clear ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR
                    : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
                .Clear = D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS {
                    .ClearValue = D3D12_CLEAR_VALUE {
                        .Format = ToDxFormat(texture_dx->Desc().format),
                        .Color = {desc.colors[i].clear_color.r, desc.colors[i].clear_color.g,
                            desc.colors[i].clear_color.b, desc.colors[i].clear_color.a}
                    }
                }
            },
            .EndingAccess = D3D12_RENDER_PASS_ENDING_ACCESS {
                .Type = desc.colors[i].store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                    : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
                .Resolve = {}, // TODO - support resolve
            }
        };
    }

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_desc;
    bool has_depth_stencil = desc.depth_stencil.has_value();
    if (has_depth_stencil) {
        const auto &depth_stencil = desc.depth_stencil.value();
        auto texture_dx = depth_stencil.texture.texture.CastTo<TextureD3D12>();
        depth_stencil_desc = D3D12_RENDER_PASS_DEPTH_STENCIL_DESC {
            .cpuDescriptor = texture_dx->GetView(TextureRenderTargetViewD3D12Desc {
                .base_layer = depth_stencil.texture.base_layer,
                .layers = depth_stencil.texture.layers,
                .base_level = depth_stencil.texture.base_level,
                .levels = depth_stencil.texture.levels,
            }).cpu,
            .DepthBeginningAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS {
                .Type = depth_stencil.clear ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR
                    : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
                .Clear = D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS {
                    .ClearValue = D3D12_CLEAR_VALUE {
                        .Format = ToDxFormat(texture_dx->Desc().format),
                        .DepthStencil = D3D12_DEPTH_STENCIL_VALUE {
                            .Depth = depth_stencil.clear_depth,
                        }
                    }
                }
            },
            .StencilBeginningAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS {
                .Type = depth_stencil.clear ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR
                    : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
                .Clear = D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS {
                    .ClearValue = D3D12_CLEAR_VALUE {
                        .Format = ToDxFormat(texture_dx->Desc().format),
                        .DepthStencil = D3D12_DEPTH_STENCIL_VALUE {
                            .Stencil = static_cast<UINT8>(depth_stencil.clear_stencil),
                        }
                    }
                }
            },
            .DepthEndingAccess = D3D12_RENDER_PASS_ENDING_ACCESS {
                .Type = depth_stencil.store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                    : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
                .Resolve = {}, // TODO - support resolve
            },
            .StencilEndingAccess = D3D12_RENDER_PASS_ENDING_ACCESS {
                .Type = depth_stencil.store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                    : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD,
                .Resolve = {},
            }
        };
    }

    cmd_list_->BeginRenderPass(colors_desc.size(), colors_desc.data(),
        has_depth_stencil ? &depth_stencil_desc : nullptr, D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES);
}

RenderCommandEncoderD3D12::~RenderCommandEncoderD3D12() {
    cmd_list_->EndRenderPass();

    if (!label_.empty()) {
        PopLabel();
    }
    base_encoder_->cmd_list_ = cmd_list_;
}

Ptr<CommandBuffer> RenderCommandEncoderD3D12::Finish() {
    cmd_list_->Close();
    auto cmd_buffer = Ptr<CommandBufferD3D12>::Make(device_, cmd_list_);
    cmd_list_ = nullptr;
    return cmd_buffer;
}

void RenderCommandEncoderD3D12::PushLabel(const CommandLabel &label) {
    UINT64 color = EncodeEventColor(label.color.r, label.color.g, label.color.b);
    PIXBeginEvent(cmd_list_, color, label.label.c_str());
}

void RenderCommandEncoderD3D12::PopLabel() {
    PIXEndEvent(cmd_list_);
}

void RenderCommandEncoderD3D12::SetPipeline(Ref<RenderPipeline> pipeline) {
    curr_pipeline_ = pipeline.CastTo<RenderPipelineD3D12>().Get();
    cmd_list_->SetPipelineState(curr_pipeline_->RawPipeline());
    cmd_list_->SetGraphicsRootSignature(curr_pipeline_->RawRootSignature());
    cmd_list_->IASetPrimitiveTopology(curr_pipeline_->RawPrimitiveTopology());
}

void RenderCommandEncoderD3D12::BindShaderParams(uint32_t set_index, const ShaderParams &values) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindShaderParams() without setting pipeline");

    DescriptorHandle descriptor_set =
        base_encoder_->context_->GetDescriptorSet(curr_pipeline_->Desc().layout.sets_layout[set_index], values);

    cmd_list_->SetGraphicsRootDescriptorTable(set_index, descriptor_set.gpu);
}

void RenderCommandEncoderD3D12::PushConstants(const void *data, uint32_t size, uint32_t offset) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::PushConstants() without setting pipeline");

    cmd_list_->SetGraphicsRoot32BitConstants(curr_pipeline_->Desc().layout.sets_layout.size(),
        size / 4, data, offset / 4);
}

void RenderCommandEncoderD3D12::SetViewports(Span<Viewport> viewports) {
    Vec<D3D12_VIEWPORT> viewports_dx(viewports.Size());
    for (size_t i = 0; i < viewports_dx.size(); i++) {
        viewports_dx[i] = D3D12_VIEWPORT {
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

void RenderCommandEncoderD3D12::SetScissors(Span<Scissor> scissors) {
    Vec<D3D12_RECT> scissors_dx(scissors.Size());
    for (size_t i = 0; i < scissors_dx.size(); i++) {
        scissors_dx[i] = D3D12_RECT {
            .left = static_cast<LONG>(scissors[i].x),
            .top = static_cast<LONG>(scissors[i].y),
            .right = static_cast<LONG>(scissors[i].x + scissors[i].width),
            .bottom = static_cast<LONG>(scissors[i].y + scissors[i].height),
        };
    }
    cmd_list_->RSSetScissorRects(scissors_dx.size(), scissors_dx.data());
}

void RenderCommandEncoderD3D12::BindVertexBuffer(Span<BufferRange> buffers, uint32_t first_binding) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindVertexBuffer() without setting pipeline");

    Vec<D3D12_VERTEX_BUFFER_VIEW> views(buffers.Size());
    for (size_t i = 0; i < buffers.Size(); i++) {
        auto buffer_dx = buffers[i].buffer.CastTo<BufferD3D12>();
        views[i] = D3D12_VERTEX_BUFFER_VIEW {
            .BufferLocation = buffer_dx->Raw()->GetGPUVirtualAddress() + buffers[i].offset,
            .SizeInBytes = static_cast<uint32_t>(buffer_dx->Size() - buffers[i].offset),
            .StrideInBytes = curr_pipeline_->Desc().vertex_input_buffers[i].stride,
        };
    }
    cmd_list_->IASetVertexBuffers(first_binding, views.size(), views.data());
}

void RenderCommandEncoderD3D12::BindIndexBuffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindIndexBuffer() without setting pipeline");

    auto buffer_dx = buffer.CastTo<BufferD3D12>();
    D3D12_INDEX_BUFFER_VIEW view {
        .BufferLocation = buffer_dx->Raw()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = static_cast<uint32_t>(buffer_dx->Size() - offset),
        .Format = ToDxIndexFormat(index_type),
    };
    cmd_list_->IASetIndexBuffer(&view);
}

void RenderCommandEncoderD3D12::Draw(uint32_t num_vertices, uint32_t num_instance, uint32_t first_vertex,
    uint32_t first_instance) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::Draw() without setting pipeline");

    cmd_list_->DrawInstanced(num_vertices, num_instance, first_vertex, first_instance);
}

void RenderCommandEncoderD3D12::DrawIndexed(uint32_t num_indices, uint32_t num_instance,
    uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::DrawIndexed() without setting pipeline");
        
    cmd_list_->DrawIndexedInstanced(num_indices, num_instance, first_index, vertex_offset, first_instance);
}


ComputeCommandEncoderD3D12::ComputeCommandEncoderD3D12(Ref<DeviceD3D12> device, Ref<CommandEncoderD3D12> base_encoder,
    const std::string &label) : device_(device), base_encoder_(base_encoder), label_(label) {
    cmd_list_ = base_encoder->cmd_list_;
}

ComputeCommandEncoderD3D12::~ComputeCommandEncoderD3D12() {
    if (!label_.empty()) {
        PopLabel();
    }
    base_encoder_->cmd_list_ = cmd_list_;
}

Ptr<CommandBuffer> ComputeCommandEncoderD3D12::Finish() {
    cmd_list_->Close();
    auto cmd_buffer = Ptr<CommandBufferD3D12>::Make(device_, cmd_list_);
    cmd_list_ = nullptr;
    return cmd_buffer;
}

void ComputeCommandEncoderD3D12::PushLabel(const CommandLabel &label) {
    UINT64 color = EncodeEventColor(label.color.r, label.color.g, label.color.b);
    PIXBeginEvent(cmd_list_, color, label.label.c_str());
}

void ComputeCommandEncoderD3D12::PopLabel() {
    PIXEndEvent(cmd_list_);
}

void ComputeCommandEncoderD3D12::SetPipeline(Ref<ComputePipeline> pipeline) {
    curr_pipeline_ = pipeline.CastTo<ComputePipelineD3D12>().Get();
    cmd_list_->SetPipelineState(curr_pipeline_->RawPipeline());
    cmd_list_->SetGraphicsRootSignature(curr_pipeline_->RawRootSignature());
}

void ComputeCommandEncoderD3D12::BindShaderParams(uint32_t set_index, const ShaderParams &values) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::BindShaderParams() without setting pipeline");

    DescriptorHandle descriptor_set =
        base_encoder_->context_->GetDescriptorSet(curr_pipeline_->Desc().layout.sets_layout[set_index], values);

    cmd_list_->SetComputeRootDescriptorTable(set_index, descriptor_set.gpu);
}

void ComputeCommandEncoderD3D12::PushConstants(const void *data, uint32_t size, uint32_t offset) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::PushConstants() without setting pipeline");

    cmd_list_->SetGraphicsRoot32BitConstants(curr_pipeline_->Desc().layout.sets_layout.size(),
        size / 4, data, offset / 4);
}

void ComputeCommandEncoderD3D12::Dispatch(uint32_t size_x, uint32_t size_y, uint32_t size_z) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::Dispatch() without setting pipeline");
    uint32_t x = (size_x + curr_pipeline_->LocalSizeX() - 1) / curr_pipeline_->LocalSizeX();
    uint32_t y = (size_y + curr_pipeline_->LocalSizeY() - 1) / curr_pipeline_->LocalSizeY();
    uint32_t z = (size_z + curr_pipeline_->LocalSizeZ() - 1) / curr_pipeline_->LocalSizeZ();
    cmd_list_->Dispatch(x, y, z);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

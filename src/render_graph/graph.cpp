#include "graph.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

RenderGraph::RenderGraph(Ref<gfx::Device> device, Ref<gfx::Queue> queue, uint32_t num_frames)
    : device_(device), queue_(queue) {
    contexts_.reserve(num_frames);
    resource_pools_.reserve(num_frames);
    for (uint32_t i = 0; i < num_frames; i++) {
        contexts_.emplace_back(std::move(device->CreateFrameContext()));
        resource_pools_.emplace_back(Ptr<ResourcePool>::Make(device));
    }
}

BufferHandle RenderGraph::AddBuffer(const std::string &name, const std::function<void(BufferBuilder &)> &setup_func) {
    BufferBuilder builder {};
    setup_func(builder);
    gfx::BufferDesc desc = builder;
    desc.name = name;

    auto node = Ptr<BufferNode>::Make();
    node->index = graph_nodes_.size();
    node->name = name;
    node->desc = desc;
    graph_nodes_.emplace_back(std::move(node));

    BufferHandle handle;
    handle.node_index_ = graph_nodes_.size() - 1;
    return handle;
}

BufferHandle RenderGraph::ImportBuffer(const std::string &name, Ref<gfx::Buffer> buffer) {
    auto node = Ptr<BufferNode>::Make();
    node->index = graph_nodes_.size();
    node->name = name;
    node->buffer = ResourcePool::Buffer {
        .buffer = buffer,
        .index = static_cast<size_t>(-1),
        .access_type = gfx::ResourceAccessType::eNone,
    };
    node->imported = true;
    graph_nodes_.emplace_back(std::move(node));

    BufferHandle handle;
    handle.node_index_ = graph_nodes_.size() - 1;
    return handle;
}

TextureHandle RenderGraph::AddTexture(const std::string &name, const std::function<void(TextureBuilder &)> &setup_func) {
    TextureBuilder builder {};
    setup_func(builder);
    gfx::TextureDesc desc = builder;
    desc.name = name;

    auto node = Ptr<TextureNode>::Make();
    node->index = graph_nodes_.size();
    node->name = name;
    node->desc = desc;
    graph_nodes_.emplace_back(std::move(node));

    TextureHandle handle;
    handle.node_index_ = graph_nodes_.size() - 1;
    return handle;
}

TextureHandle RenderGraph::ImportTexture(const std::string &name, Ref<gfx::Texture> texture) {
    auto node = Ptr<TextureNode>::Make();
    node->index = graph_nodes_.size();
    node->name = name;
    node->texture = ResourcePool::Texture {
        .texture = texture,
        .index = static_cast<size_t>(-1),
        .access_type = gfx::ResourceAccessType::eNone,
    };
    node->imported = true;
    graph_nodes_.emplace_back(std::move(node));

    TextureHandle handle;
    handle.node_index_ = graph_nodes_.size() - 1;
    return handle;
}

void RenderGraph::AddRenderPass(const std::string &name,
    const std::function<void(struct RenderPassBuilder &)> &setup_func,
    const std::function<void(Ref<gfx::RenderCommandEncoder>, const PassResource &)> &execute_func) {
    RenderPassBuilder builder {};
    setup_func(builder);

    auto node = Ptr<RenderPassNode>::Make();
    node->index = graph_nodes_.size();
    node->name = name;
    node->execute_func = execute_func;
    node->resource.graph_ = this;
    node->resource.read_buffers_ = std::move(builder.read_buffers_);
    node->resource.read_buffers_type_ = std::move(builder.read_buffers_type_);
    node->resource.write_buffers_ = std::move(builder.write_buffers_);
    node->resource.read_textures_ = std::move(builder.read_textures_);
    node->resource.write_textures_ = std::move(builder.write_textures_);
    std::copy_n(builder.color_targets_, gfx::kMaxRenderTargetsCount, node->color_targets);
    node->depth_stencil_target = builder.depth_stencil_target_;

    for (const auto &[_, handle] : node->resource.read_buffers_) {
        AddEdge(graph_nodes_[handle.node_index_], node.AsRef());
    }
    for (const auto &[_, handle] : node->resource.read_textures_) {
        AddEdge(graph_nodes_[handle.node_index_], node.AsRef());
    }
    for (const auto &[_, handle] : node->resource.write_buffers_) {
        AddEdge(node.AsRef(), graph_nodes_[handle.node_index_]);
    }
    for (const auto &[_, handle] : node->resource.write_textures_) {
        AddEdge(node.AsRef(), graph_nodes_[handle.node_index_]);
    }
    for (const auto &target_opt : builder.color_targets_) {
        if (target_opt.has_value()) {
            const auto &target = target_opt.value();
            AddEdge(node.AsRef(), graph_nodes_[target.handle.node_index_]);
        }
    }
    if (builder.depth_stencil_target_.has_value()) {
        const auto &target = builder.depth_stencil_target_.value();
        AddEdge(node.AsRef(), graph_nodes_[target.handle.node_index_]);
    }

    graph_nodes_.emplace_back(std::move(node));
}

void RenderGraph::AddComputePass(const std::string &name,
    const std::function<void(struct ComputePassBuilder &)> &setup_func,
    const std::function<void(Ref<gfx::ComputeCommandEncoder>, const PassResource &)> &execute_func) {
    ComputePassBuilder builder {};
    setup_func(builder);

    auto node = Ptr<ComputePassNode>::Make();
    node->index = graph_nodes_.size();
    node->name = name;
    node->execute_func = execute_func;
    node->resource.graph_ = this;
    node->resource.read_buffers_ = std::move(builder.read_buffers_);
    node->resource.read_buffers_type_ = std::move(builder.read_buffers_type_);
    node->resource.write_buffers_ = std::move(builder.write_buffers_);
    node->resource.read_textures_ = std::move(builder.read_textures_);
    node->resource.write_textures_ = std::move(builder.write_textures_);

    for (const auto &[_, handle] : node->resource.read_buffers_) {
        AddEdge(graph_nodes_[handle.node_index_], node.AsRef());
    }
    for (const auto &[_, handle] : node->resource.read_textures_) {
        AddEdge(graph_nodes_[handle.node_index_], node.AsRef());
    }
    for (const auto &[_, handle] : node->resource.write_buffers_) {
        AddEdge(node.AsRef(), graph_nodes_[handle.node_index_]);
    }
    for (const auto &[_, handle] : node->resource.write_textures_) {
        AddEdge(node.AsRef(), graph_nodes_[handle.node_index_]);
    }

    graph_nodes_.emplace_back(std::move(node));
}

void RenderGraph::AddPresentPass(TextureHandle texture) {
    auto node = Ptr<PresentPassNode>::Make();
    node->index = graph_nodes_.size();
    node->name = "present pass";
    node->texture = texture;
    AddEdge(graph_nodes_[texture.node_index_], node.AsRef());
    present_pass_index_ = graph_nodes_.size();
    graph_nodes_.emplace_back(std::move(node));
}

void RenderGraph::AddEdge(Ref<Node> from, Ref<Node> to) {
    from->out_nodes.push_back(to);
    to->in_nodes.push_back(from);
}

void RenderGraph::Compile() {
    // cull unused passes using prsent pass node
    Vec<bool> used(graph_nodes_.size(), false);
    Vec<size_t> queue(graph_nodes_.size(), 0);
    size_t ql = 0, qr = 0;
    if (present_pass_index_ < graph_nodes_.size()) {
        queue[qr++] = present_pass_index_;
        used[present_pass_index_] = true;
        while (ql < qr) {
            size_t index = queue[ql++];
            for (const auto &v : graph_nodes_[index]->in_nodes) {
                if (!used[v->index]) {
                    used[v->index] = true;
                    queue[qr++] = v->index;
                }
            }
        }
        for (size_t i = 0; i < graph_nodes_.size(); i++) {
            if (used[i] && !graph_nodes_[i]->IsResource()) {
                for (const auto &v : graph_nodes_[i]->out_nodes) {
                    used[v->index] = true;
                }
            }
        }
    } else {
        std::fill(used.begin(), used.end(), true);
    }

    // toposort
    graph_order_.clear();
    graph_order_.reserve(graph_nodes_.size());
    resources_to_create_.resize(graph_nodes_.size(), {});
    resources_to_destroy_.resize(graph_nodes_.size(), {});

    queue.resize(graph_nodes_.size(), 0);
    ql = 0;
    qr = 0;
    Vec<size_t> in_degs(graph_nodes_.size(), 0);
    for (size_t i = 0; i < graph_nodes_.size(); i++) {
        if (!used[i]) {
            continue;
        }
        in_degs[i] = graph_nodes_[i]->in_nodes.size();
        if (in_degs[i] == 0) {
            queue[qr++] = i;
        }
    }

    Vec<size_t> order_of(graph_nodes_.size(), 0);
    while (ql < qr) {
        size_t index = queue[ql++];
        order_of[index] = graph_order_.size();
        graph_order_.push_back(index);

        for (const auto &v : graph_nodes_[index]->out_nodes) {
            if (used[v->index] && --in_degs[v->index] == 0) {
                queue[qr++] = v->index;
            }
        }
    }

    for (const auto &node : graph_nodes_) {
        if (node->IsResource()) {
            size_t start = graph_nodes_.size();
            for (const auto &v : node->in_nodes) {
                start = std::min(start, order_of[v->index]);
            }
            resources_to_create_[start].push_back(node->index);

            size_t end = 0;
            for (const auto &v : node->out_nodes) {
                end = std::max(end, order_of[v->index]);
            }
            resources_to_destroy_[end].push_back(node->index);
        }
    }
}

void RenderGraph::BufferNode::Create(RenderGraph &rg) {
    if (!buffer.has_value()) {
        buffer = rg.resource_pools_[rg.curr_frame_]->GetBuffer(desc);
    }
}

void RenderGraph::BufferNode::Destroy(RenderGraph &rg) {
    if (!imported && buffer.has_value()) {
        rg.resource_pools_[rg.curr_frame_]->RemoveBuffer(desc, buffer.value());
        buffer = std::nullopt;
    }
}

void RenderGraph::TextureNode::Create(RenderGraph &rg) {
    if (!texture.has_value()) {
        texture = rg.resource_pools_[rg.curr_frame_]->GetTexure(desc);
    }
}

void RenderGraph::TextureNode::Destroy(RenderGraph &rg) {
    if (!imported && texture.has_value()) {
        rg.resource_pools_[rg.curr_frame_]->RemoveTexture(desc, texture.value());
        texture = std::nullopt;
    }
}

void RenderGraph::RenderPassNode::SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) {
    Vec<gfx::BufferBarrier> buffer_barriers;
    Vec<gfx::TextureBarrier> texture_barriers;

    resource.GetShaderBarriers(rg, true, buffer_barriers, texture_barriers);

    auto target_access_type = gfx::ResourceAccessType::eColorAttachmentWrite;
    for (size_t i = 0; i < gfx::kMaxRenderTargetsCount; i++) {
        const auto &target_opt = color_targets[i];
        if (!target_opt.has_value()) {
            continue;
        }

        const auto &target = target_opt.value();
        auto &texture = rg.Texture(target.handle);
        if (target_access_type != texture.access_type) {
            texture_barriers.push_back(gfx::TextureBarrier {
                .texture = { texture.texture },
                .src_access_type = texture.access_type,
                .dst_access_type = target_access_type,
            });
            texture.access_type = target_access_type;
        }
    }
    if (depth_stencil_target.has_value()) {
        target_access_type = gfx::ResourceAccessType::eDepthStencilAttachmentWrite;
        const auto &target = depth_stencil_target.value();
        auto &texture = rg.Texture(target.handle);
        if (target_access_type != texture.access_type) {
            texture_barriers.push_back(gfx::TextureBarrier {
                .texture = { texture.texture },
                .src_access_type = texture.access_type,
                .dst_access_type = target_access_type,
            });
            texture.access_type = target_access_type;
        }
    }

    cmd_encoder->ResourceBarrier(buffer_barriers, texture_barriers);
}

void RenderGraph::RenderPassNode::Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const {
    gfx::RenderTargetDesc rt_desc {};
    size_t num_color_targets = gfx::kMaxRenderTargetsCount;
    while (num_color_targets > 0) {
        if (!color_targets[num_color_targets - 1].has_value()) {
            --num_color_targets;
        } else {
            break;
        }
    }
    rt_desc.colors.reserve(num_color_targets);
    for (size_t i = 0; i < num_color_targets; i++) {
        const auto &target = color_targets[i].value();
        rt_desc.colors.push_back(gfx::ColorAttachmentDesc {
            .texture = {
                .texture = rg.Texture(target.handle).texture,
                .base_level = target.level,
                .levels = 1,
                .base_layer = target.base_layer,
                .layers = target.layers,
            },
            .clear_color = {
                target.clear_color.r,
                target.clear_color.g,
                target.clear_color.b,
                target.clear_color.a
            },
            .clear = target.clear,
            .store = target.store,
        });
    }
    if (depth_stencil_target.has_value()) {
        const auto &target = depth_stencil_target.value();
        rt_desc.depth_stencil = gfx::DepthStencilAttachmentDesc {
            .texture = {
                .texture = rg.Texture(target.handle).texture,
                .base_level = target.level,
                .levels = 1,
                .base_layer = target.base_layer,
                .layers = target.layers,
            },
            .clear_depth = target.clear_depth,
            .clear_stencil = target.clear_stencil,
            .clear = target.clear,
            .store = target.store,
        };
    }
    auto render_encoder = cmd_encoder->BeginRenderPass({ name }, rt_desc);
    execute_func(render_encoder.AsRef(), resource);
}

void RenderGraph::ComputePassNode::SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) {
    Vec<gfx::BufferBarrier> buffer_barriers;
    Vec<gfx::TextureBarrier> texture_barriers;

    resource.GetShaderBarriers(rg, false, buffer_barriers, texture_barriers);

    cmd_encoder->ResourceBarrier(buffer_barriers, texture_barriers);
}

void RenderGraph::ComputePassNode::Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const {
    auto compute_encoder = cmd_encoder->BeginComputePass({ name });
    execute_func(compute_encoder.AsRef(), resource);
}

void RenderGraph::PresentPassNode::SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) {
    auto target_access_type = gfx::ResourceAccessType::ePresent;
    auto &rg_texture = rg.Texture(texture);
    if (rg_texture.access_type != target_access_type) {
        gfx::TextureBarrier barrier {
            .texture = { rg_texture.texture },
            .src_access_type = rg_texture.access_type,
            .dst_access_type = target_access_type,
        };
        cmd_encoder->ResourceBarrier({}, { barrier });
    }
}

void RenderGraph::Execute(Span<Ref<gfx::Semaphore>> wait_semaphores, Span<Ref<gfx::Semaphore>> signal_semaphores,
    gfx::Fence *signal_fence) {
    contexts_[curr_frame_]->Reset();
    auto cmd_encoder = contexts_[curr_frame_]->GetCommandEncoder();

    for (size_t order = 0; order < graph_order_.size(); order++) {
        for (const size_t resource_index : resources_to_create_[order]) {
            const auto &resource_node = graph_nodes_[resource_index];
            resource_node->Create(*this);
        }

        const auto &node = graph_nodes_[graph_order_[order]];
        node->SetBarriers(cmd_encoder, *this);
        node->Execute(cmd_encoder, *this);

        for (const size_t resource_index : resources_to_destroy_[order]) {
            const auto &resource_node = graph_nodes_[resource_index];
            resource_node->Destroy(*this);
        }
    }

    queue_->SubmitCommandBuffer({ cmd_encoder->Finish() }, wait_semaphores, signal_semaphores, signal_fence);

    Clear();
    curr_frame_ = (curr_frame_ + 1) % contexts_.size();
}

void RenderGraph::Clear() {
    graph_nodes_.clear();
    graph_order_.clear();
    resources_to_create_.clear();
    resources_to_destroy_.clear();
    present_pass_index_ = static_cast<size_t>(-1);
}

ResourcePool::Buffer &RenderGraph::Buffer(BufferHandle handle) {
    return graph_nodes_[handle.node_index_].AsRef().CastTo<BufferNode>()->buffer.value();
}
const ResourcePool::Buffer &RenderGraph::Buffer(BufferHandle handle) const {
    return graph_nodes_[handle.node_index_].AsRef().CastTo<BufferNode>()->buffer.value();
}

ResourcePool::Texture &RenderGraph::Texture(TextureHandle handle) {
    return graph_nodes_[handle.node_index_].AsRef().CastTo<TextureNode>()->texture.value();
}
const ResourcePool::Texture &RenderGraph::Texture(TextureHandle handle) const {
    return graph_nodes_[handle.node_index_].AsRef().CastTo<TextureNode>()->texture.value();
}

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END

#include <bisemutum/graphics/render_graph.hpp>

#include <array>
#include <unordered_map>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/graphics/render_graph_pass.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/prelude/hash.hpp>

namespace bi::gfx {

namespace {

struct BufferKey final {
    auto operator==(BufferKey const& rhs) const -> bool = default;

    uint32_t size_log;
    rhi::BufferMemoryProperty memory_property;
    BitFlags<rhi::BufferUsage> usages;
};

}

}

template <>
struct std::hash<bi::gfx::BufferKey> final {
    auto operator()(bi::gfx::BufferKey const& v) const noexcept -> size_t {
        return bi::hash(v.size_log, v.memory_property, v.usages.raw_value());
    }
};

template <>
struct std::hash<bi::rhi::TextureDesc> final {
    auto operator()(bi::rhi::TextureDesc const& v) const noexcept -> size_t {
        return bi::hash(
            v.extent.width, v.extent.height, v.extent.depth_or_layers, v.levels,
            v.dim, v.format, v.usages.raw_value()
        );
    }
};

namespace bi::gfx {

namespace {

struct PoolBuffer final {
    Ref<Buffer> buffer;
    size_t index;
    BitFlags<rhi::ResourceAccessType> access;
};
struct PoolTexture final {
    Ref<Texture> texture;
    size_t index;
    BitFlags<rhi::ResourceAccessType> access;
};

struct BufferPool final {
    std::vector<Buffer> resources;
    std::vector<BitFlags<rhi::ResourceAccessType>> accesses;
    std::vector<size_t> recycled_indices;
};
struct TexturePool final {
    std::vector<Texture> resources;
    std::vector<BitFlags<rhi::ResourceAccessType>> accesses;
    std::vector<size_t> recycled_indices;
};

auto need_barrier(BitFlags<rhi::ResourceAccessType> from, BitFlags<rhi::ResourceAccessType> to) -> bool {
    return from != to
        || (
            from.contains_any(rhi::ResourceAccessType::storage_resource_write)
            && to.contains_any(rhi::ResourceAccessType::storage_resource_write)
        );
}

} // namespace

struct RenderGraph::Impl final {
    struct Node {
        std::vector<Ref<Node>> in_nodes;
        std::vector<Ref<Node>> out_nodes;
        std::string name;
        size_t index;

        virtual ~Node() = default;

        virtual auto is_resource() const -> bool { return false; }

        virtual auto set_barriers(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl& rg) -> void {}
        virtual auto execute(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg) const -> void {}

        virtual auto create(RenderGraph::Impl& rg) -> void {}
        virtual auto destroy(RenderGraph::Impl& rg) -> void {}
    };
    struct BufferNode final : Node {
        rhi::BufferDesc desc;
        Option<PoolBuffer> buffer;
        bool imported = false;

        auto is_resource() const -> bool override { return true; }

        auto create(RenderGraph::Impl& rg) -> void override;
        auto destroy(RenderGraph::Impl& rg) -> void override;
    };
    struct TextureNode final : Node {
        rhi::TextureDesc desc;
        Option<PoolTexture> texture;
        bool imported = false;

        auto is_resource() const -> bool override { return true; }

        auto create(RenderGraph::Impl& rg) -> void override;
        auto destroy(RenderGraph::Impl& rg) -> void override;
    };
    struct GraphicsPassNode final : Node {
        GraphicsPassBuilder builder;
        std::any pass_data;

        auto set_barriers(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl& rg) -> void override;
        auto execute(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg) const -> void override;
    };
    struct ComputePassNode final : Node {
        ComputePassBuilder builder;
        std::any pass_data;

        auto set_barriers(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl& rg) -> void override;
        auto execute(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg) const -> void override;
    };
    struct BlitPassNode final : Node {
        TextureHandle src;
        uint32_t src_mip_level;
        uint32_t src_array_layer;
        TextureHandle dst;
        uint32_t dst_mip_level;
        uint32_t dst_array_layer;

        auto set_barriers(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl& rg) -> void override;
        auto execute(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg) const -> void override;
    };
    struct PresentPassNode final : Node {
        TextureHandle texture;

        auto set_barriers(Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl& rg) -> void override;
    };

    auto add_buffer(std::function<auto(BufferBuilder&) -> void>&& setup_func) -> BufferHandle {
        BufferBuilder builder{};
        setup_func(builder);

        auto node = Box<BufferNode>::make();
        node->index = graph_nodes_.size();
        node->desc = builder;
        graph_nodes_.push_back(std::move(node));

        return static_cast<BufferHandle>(graph_nodes_.size() - 1);
    }
    auto import_buffer(Ref<Buffer> buffer) -> BufferHandle {
        auto node = Box<BufferNode>::make();
        node->index = graph_nodes_.size();
        node->buffer = PoolBuffer{
            .buffer = buffer,
            .index = static_cast<size_t>(-1),
            .access = rhi::ResourceAccessType::none,
        };
        node->imported = true;
        graph_nodes_.push_back(std::move(node));

        return static_cast<BufferHandle>(graph_nodes_.size() - 1);
    }
    auto add_texture(std::function<auto(TextureBuilder&) -> void>&& setup_func) -> TextureHandle {
        TextureBuilder builder{};
        setup_func(builder);

        auto node = Box<TextureNode>::make();
        node->index = graph_nodes_.size();
        node->desc = builder;
        graph_nodes_.push_back(std::move(node));

        return static_cast<TextureHandle>(graph_nodes_.size() - 1);
    }
    auto import_texture(Ref<Texture> texture, BitFlags<rhi::ResourceAccessType> access) -> TextureHandle {
        auto node = Box<TextureNode>::make();
        node->index = graph_nodes_.size();
        node->texture = PoolTexture{
            .texture = texture,
            .index = static_cast<size_t>(-1),
            .access = access,
        };
        node->imported = true;
        graph_nodes_.push_back(std::move(node));

        return static_cast<TextureHandle>(graph_nodes_.size() - 1);
    }
    auto import_back_buffer() const -> TextureHandle {
        return back_buffer_handle_;
    }

    auto add_graphics_pass(
        RenderGraph* rg, std::string_view name, std::any&& pass_data
    ) -> std::pair<GraphicsPassBuilder&, std::any*> {
        auto node = Box<GraphicsPassNode>::make();
        node->index = graph_nodes_.size();
        node->name = name;
        node->pass_data = std::move(pass_data);
        node->builder.rg_ = rg;
        node->builder.pass_index_ = graph_nodes_.size();
        auto builder = &node->builder;
        auto p_pass_data = &node->pass_data;
        graph_nodes_.push_back(std::move(node));
        return {*builder, p_pass_data};
    }

    auto add_compute_pass(
        RenderGraph* rg, std::string_view name, std::any&& pass_data
    ) -> std::pair<ComputePassBuilder&, std::any*> {
        auto node = Box<ComputePassNode>::make();
        node->index = graph_nodes_.size();
        node->name = name;
        node->pass_data = std::move(pass_data);
        node->builder.rg_ = rg;
        node->builder.pass_index_ = graph_nodes_.size();
        auto builder = &node->builder;
        auto p_pass_data = &node->pass_data;
        graph_nodes_.push_back(std::move(node));
        return {*builder, p_pass_data};
    }

    auto add_blit_pass(
        RenderGraph* rg, std::string_view name,
        TextureHandle src, uint32_t src_mip_level, uint32_t src_array_layer,
        TextureHandle dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void {
        auto node = Box<BlitPassNode>::make();
        node->index = graph_nodes_.size();
        node->name = name;
        node->src = src;
        node->src_mip_level = src_mip_level;
        node->src_array_layer = src_array_layer;
        node->dst = dst;
        node->dst_mip_level = dst_mip_level;
        node->dst_array_layer = dst_array_layer;
        graph_nodes_.push_back(std::move(node));
        rg->add_read_edge(graph_nodes_.size() - 1, src);
        rg->add_write_edge(graph_nodes_.size() - 1, dst);
    }

    auto add_present_pass(TextureHandle texture) -> void {
        auto node = Box<PresentPassNode>::make();
        node->index = graph_nodes_.size();
        node->name = "present pass";
        node->texture = texture;
        add_edge(graph_nodes_[static_cast<size_t>(texture)].ref(), node.ref());
        present_pass_index_ = graph_nodes_.size();
        graph_nodes_.emplace_back(std::move(node));
    }

    auto add_rendered_object_list(RenderedObjectListDesc const& desc) -> RenderedObjectListHandle {
        std::vector<Ref<Drawable>> drawables;
        auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<GpuSceneSystem>();
        gpu_scene->for_each_drawable([this, &drawables, &desc](Drawable& drawable) {
            auto mat_is_opaque = drawable.material->blend_mode == BlendMode::opaque
                || drawable.material->blend_mode == BlendMode::alpha_test;
            if (
                (desc.type.contains_any(RenderedObjectType::opaque) && mat_is_opaque)
                || (desc.type.contains_any(RenderedObjectType::transparent) && !mat_is_opaque)
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

        auto& list = rendered_object_lists_.emplace_back(desc.camera, desc.fragmeng_shader);
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
                RenderedObjectListItem item{};
                item.drawables.reserve(j - i + 1);
                for (auto k = i; k <= j; k++) {
                    item.drawables.push_back(drawables[k]);
                }
                list.items.push_back(std::move(item));
                i = j + 1;
            }
        }

        return static_cast<RenderedObjectListHandle>(rendered_object_lists_.size() - 1);
    }
    auto rendered_object_list(RenderedObjectListHandle handle) const -> CRef<RenderedObjectList> {
        return rendered_object_lists_[static_cast<size_t>(handle)];
    }

    auto compile() -> void {
        if (present_pass_index_ >= graph_nodes_.size()) {
            graph_is_invalid = true;
            return;
        }

        // cull unused passes using prsent pass node
        std::vector<bool> used(graph_nodes_.size(), false);
        std::vector<size_t> queue(graph_nodes_.size(), 0);
        size_t ql = 0, qr = 0;
        if (present_pass_index_ < graph_nodes_.size()) {
            queue[qr++] = present_pass_index_;
            used[present_pass_index_] = true;
            while (ql < qr) {
                size_t index = queue[ql++];
                for (auto const& v : graph_nodes_[index]->in_nodes) {
                    if (!used[v->index]) {
                        used[v->index] = true;
                        queue[qr++] = v->index;
                    }
                }
            }
            for (size_t i = 0; i < graph_nodes_.size(); i++) {
                if (used[i] && !graph_nodes_[i]->is_resource()) {
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
        std::vector<size_t> in_degs(graph_nodes_.size(), 0);
        for (size_t i = 0; i < graph_nodes_.size(); i++) {
            if (!used[i]) {
                continue;
            }
            in_degs[i] = graph_nodes_[i]->in_nodes.size();
            if (in_degs[i] == 0) {
                queue[qr++] = i;
            }
        }

        std::vector<size_t> order_of(graph_nodes_.size(), 0);
        while (ql < qr) {
            size_t index = queue[ql++];
            order_of[index] = graph_order_.size();
            graph_order_.push_back(index);

            for (auto const& v : graph_nodes_[index]->out_nodes) {
                if (used[v->index] && --in_degs[v->index] == 0) {
                    queue[qr++] = v->index;
                }
            }
        }

        for (auto const& node : graph_nodes_) {
            if (node->is_resource()) {
                size_t start = graph_nodes_.size();
                for (auto const& v : node->in_nodes) {
                    start = std::min(start, order_of[v->index]);
                }
                resources_to_create_[start].push_back(node->index);

                size_t end = 0;
                for (auto const& v : node->out_nodes) {
                    end = std::max(end, order_of[v->index]);
                }
                resources_to_destroy_[end].push_back(node->index);
            }
        }

        // check
        for (auto const& node : graph_nodes_) {
            if (used[node->index] && in_degs[node->index] > 0) {
                log::warn("general", "Found cycle in render graph.");
                graph_is_invalid = true;
                break;
            }
        }
    }
    auto execute(RenderGraph& rg) -> void {
        if (graph_is_invalid) { return; }

        for (size_t order = 0; order < graph_order_.size(); order++) {
            for (const auto resource_index : resources_to_create_[order]) {
                auto const& resource_node = graph_nodes_[resource_index];
                resource_node->create(*this);
            }

            auto const& node = graph_nodes_[graph_order_[order]];
            node->set_barriers(cmd_encoder_.value(), *this);
            node->execute(cmd_encoder_.value(), rg);

            for (const auto resource_index : resources_to_destroy_[order]) {
                auto const& resource_node = graph_nodes_[resource_index];
                resource_node->destroy(*this);
            }
        }

        clear();
    }

    auto buffer(BufferHandle handle) const -> Ref<Buffer> {
        return graph_nodes_[static_cast<size_t>(handle)].ref().cast_to<BufferNode>()->buffer.value().buffer;
    }
    auto texture(TextureHandle handle) const -> Ref<Texture> {
        return graph_nodes_[static_cast<size_t>(handle)].ref().cast_to<TextureNode>()->texture.value().texture;
    }
    auto pool_buffer(BufferHandle handle) -> PoolBuffer& {
        return graph_nodes_[static_cast<size_t>(handle)].ref().cast_to<BufferNode>()->buffer.value();
    }
    auto pool_texture(TextureHandle handle) -> PoolTexture& {
        return graph_nodes_[static_cast<size_t>(handle)].ref().cast_to<TextureNode>()->texture.value();
    }

    auto set_graphics_device(Ref<rhi::Device> device, uint32_t num_frames) -> void {
        device_ = device;
        num_frames_ = num_frames;
    }
    auto set_back_buffer(Ref<Texture> texture, BitFlags<rhi::ResourceAccessType> access) -> void {
        back_buffer_handle_ = import_texture(texture, access);
        add_present_pass(back_buffer_handle_);
    }
    auto set_command_encoder(Ref<rhi::CommandEncoder> cmd_encoder) -> void {
        cmd_encoder_ = cmd_encoder;
    }

    auto add_read_edge(size_t pass_index, BufferHandle handle) -> void {
        add_edge(graph_nodes_[static_cast<size_t>(handle)].ref(), graph_nodes_[pass_index].ref());
    }
    auto add_read_edge(size_t pass_index, TextureHandle handle) -> void {
        add_edge(graph_nodes_[static_cast<size_t>(handle)].ref(), graph_nodes_[pass_index].ref());
    }
    auto add_write_edge(size_t pass_index, BufferHandle handle) -> void {
        add_edge(graph_nodes_[pass_index].ref(), graph_nodes_[static_cast<size_t>(handle)].ref());
    }
    auto add_write_edge(size_t pass_index, TextureHandle handle) -> void {
        add_edge(graph_nodes_[pass_index].ref(), graph_nodes_[static_cast<size_t>(handle)].ref());
    }

    auto clear() -> void {
        graph_nodes_.clear();
        graph_order_.clear();
        resources_to_create_.clear();
        resources_to_destroy_.clear();
        present_pass_index_ = static_cast<size_t>(-1);
        graph_is_invalid = false;

        rendered_object_lists_.clear();
    }
    auto add_edge(Ref<Node> from, Ref<Node> to) -> void {
        from->out_nodes.push_back(to);
        to->in_nodes.push_back(from);
    }

    auto require_buffer(Ref<rhi::Device> device, rhi::BufferDesc const& desc) -> PoolBuffer {
        BufferKey key{
            .size_log = 0,
            .memory_property = desc.memory_property,
            .usages = desc.usages,
        };
        auto created_size = 1;
        while (created_size < desc.size) {
            created_size <<= 1;
            ++key.size_log;
        }
        auto& pool = buffer_pools[key];
        if (!pool.recycled_indices.empty()) {
            auto index = pool.recycled_indices.back();
            pool.recycled_indices.pop_back();
            auto buffer = make_ref(pool.resources[index]);
            auto access = pool.accesses[index];
            return PoolBuffer{buffer, index, access};
        } else {
            auto index = pool.resources.size();
            pool.resources.emplace_back(desc, false);
            auto buffer = make_ref(pool.resources.back());
            pool.accesses.emplace_back();
            auto access = pool.accesses.back();
            return PoolBuffer{buffer, index, access};
        }
    }
    auto remove_buffer(PoolBuffer const& buffer, rhi::BufferDesc const& desc) -> void {
        BufferKey key{
            .size_log = 0,
            .memory_property = desc.memory_property,
            .usages = desc.usages,
        };
        auto created_size = 1;
        while (created_size < desc.size) {
            created_size <<= 1;
            ++key.size_log;
        }
        auto& pool = buffer_pools[key];
        pool.accesses[buffer.index] = buffer.access;
        pool.recycled_indices.push_back(buffer.index);
    }

    auto require_texture(Ref<rhi::Device> device, rhi::TextureDesc const& desc) -> PoolTexture {
        auto& pool = texture_pools[desc];
        if (!pool.recycled_indices.empty()) {
            auto index = pool.recycled_indices.back();
            pool.recycled_indices.pop_back();
            auto texture = make_ref(pool.resources[index]);
            auto access = pool.accesses[index];
            return PoolTexture{texture, index, access};
        } else {
            auto index = pool.resources.size();
            pool.resources.emplace_back(desc);
            auto texture = make_ref(pool.resources.back());
            pool.accesses.emplace_back();
            auto access = pool.accesses.back();
            return PoolTexture{texture, index, access};
        }
    }
    auto remove_texture(PoolTexture const& texture, rhi::TextureDesc const& desc) -> void {
        auto& pool = texture_pools[desc];
        pool.accesses[texture.index] = texture.access;
        pool.recycled_indices.push_back(texture.index);
    }

    Ptr<rhi::Device> device_;
    uint32_t num_frames_;

    std::unordered_map<BufferKey, BufferPool> buffer_pools;
    std::unordered_map<rhi::TextureDesc, TexturePool> texture_pools;

    Ptr<rhi::CommandEncoder> cmd_encoder_;

    TextureHandle back_buffer_handle_;

    std::vector<Box<Node>> graph_nodes_;
    std::vector<size_t> graph_order_;
    std::vector<std::vector<size_t>> resources_to_create_;
    std::vector<std::vector<size_t>> resources_to_destroy_;
    size_t present_pass_index_ = static_cast<size_t>(-1);
    bool graph_is_invalid = false;

    std::vector<RenderedObjectList> rendered_object_lists_;
};

auto RenderGraph::Impl::BufferNode::create(RenderGraph::Impl& rg) -> void {
    if (!buffer.has_value()) {
        buffer = rg.require_buffer(rg.device_.value(), desc);
    }
}
auto RenderGraph::Impl::BufferNode::destroy(RenderGraph::Impl& rg) -> void {
    if (!imported && buffer.has_value()) {
        rg.remove_buffer(buffer.value(), desc);
        buffer.reset();
    }
}

auto RenderGraph::Impl::TextureNode::create(RenderGraph::Impl& rg) -> void {
    if (!texture.has_value()) {
        texture = rg.require_texture(rg.device_.value(), desc);
    }
}
auto RenderGraph::Impl::TextureNode::destroy(RenderGraph::Impl& rg) -> void {
    if (!imported && texture.has_value()) {
        rg.remove_texture(texture.value(), desc);
        texture.reset();
    }
}

auto get_shader_barriers(
    RenderGraph::Impl& rg,
    std::vector<BufferHandle> const& read_buffers,
    std::vector<BufferHandle> const& write_buffers,
    std::vector<TextureHandle> const& read_textures,
    std::vector<TextureHandle> const& write_textures,
    std::vector<rhi::BufferBarrier>& buffer_barriers,
    std::vector<rhi::TextureBarrier>& texture_barriers
) -> void {
    for (auto handle : read_buffers) {
        auto& pool_buffer = rg.pool_buffer(handle);
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (pool_buffer.buffer->desc().usages.contains_any(rhi::BufferUsage::uniform)) {
            target_access.set(rhi::ResourceAccessType::uniform_buffer_read);
        } else if (pool_buffer.buffer->desc().usages.contains_any(rhi::BufferUsage::indirect)) {
            target_access.set(rhi::ResourceAccessType::indirect_read);
        } else if (pool_buffer.buffer->desc().usages.contains_any(rhi::BufferUsage::storage_read)) {
            // Only add storage_resource_read when buffer cannot be used as others
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }
        if (need_barrier(pool_buffer.access, target_access)) {
            buffer_barriers.push_back(rhi::BufferBarrier{
                .buffer = pool_buffer.buffer->rhi_buffer(),
                .src_access_type = pool_buffer.access,
                .dst_access_type = target_access,
            });
            pool_buffer.access = target_access;
        }
    }
    for (auto handle : read_textures) {
        auto& pool_texture = rg.pool_texture(handle);
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (pool_texture.texture->desc().usages.contains_any(rhi::TextureUsage::sampled)) {
            target_access.set(rhi::ResourceAccessType::sampled_texture_read);
        } else if (pool_texture.texture->desc().usages.contains_any(rhi::TextureUsage::storage_read)) {
            // Only add storage_resource_read when texture cannot be used as a sampled texture
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }
        if (need_barrier(pool_texture.access, target_access)) {
            texture_barriers.push_back(rhi::TextureBarrier{
                .texture = pool_texture.texture->rhi_texture(),
                .src_access_type = pool_texture.access,
                .dst_access_type = target_access,
            });
            pool_texture.access = target_access;
        }
    }
    for (auto handle : write_buffers) {
        auto& pool_buffer = rg.pool_buffer(handle);
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (pool_buffer.buffer->desc().usages.contains_any(rhi::BufferUsage::storage_read_write)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_write);
        }
        if (need_barrier(pool_buffer.access, target_access)) {
            buffer_barriers.push_back(rhi::BufferBarrier{
                .buffer = pool_buffer.buffer->rhi_buffer(),
                .src_access_type = pool_buffer.access,
                .dst_access_type = target_access,
            });
            pool_buffer.access = target_access;
        }
    }
    for (auto handle : write_textures) {
        auto& pool_texture = rg.pool_texture(handle);
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (pool_texture.texture->desc().usages.contains_any(rhi::TextureUsage::storage_read_write)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_write);
        }
        if (need_barrier(pool_texture.access, target_access)) {
            texture_barriers.push_back(rhi::TextureBarrier{
                .texture = pool_texture.texture->rhi_texture(),
                .src_access_type = pool_texture.access,
                .dst_access_type = target_access,
            });
            pool_texture.access = target_access;
        }
    }
}

auto RenderGraph::Impl::GraphicsPassNode::set_barriers(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl &rg
) -> void {
    std::vector<rhi::BufferBarrier> buffer_barriers{};
    std::vector<rhi::TextureBarrier> texture_barriers{};

    get_shader_barriers(
        rg,
        builder.read_buffers_,
        builder.write_buffers_,
        builder.read_textures_,
        builder.write_textures_,
        buffer_barriers,
        texture_barriers
    );

    auto target_access = rhi::ResourceAccessType::color_attachment_write;
    for (size_t i = 0; i < builder.color_targets_.size(); i++) {
        auto const& target_opt = builder.color_targets_[i];
        if (!target_opt.has_value()) { continue; }

        auto const& target = target_opt.value();
        auto& texture = rg.pool_texture(target.handle);
        if (need_barrier(target_access, texture.access)) {
            texture_barriers.push_back(rhi::TextureBarrier{
                .texture = texture.texture->rhi_texture(),
                .src_access_type = texture.access,
                .dst_access_type = target_access,
            });
            texture.access = target_access;
        }
    }
    if (builder.depth_stencil_target_.has_value()) {
        target_access = rhi::ResourceAccessType::depth_stencil_attachment_write;
        auto const& target = builder.depth_stencil_target_.value();
        auto& texture = rg.pool_texture(target.handle);
        if (need_barrier(target_access, texture.access)) {
            texture_barriers.push_back(rhi::TextureBarrier{
                .texture = texture.texture->rhi_texture(),
                .src_access_type = texture.access,
                .dst_access_type = target_access,
            });
            texture.access = target_access;
        }
    }

    cmd_encoder->resource_barriers(buffer_barriers, texture_barriers);
}
auto RenderGraph::Impl::GraphicsPassNode::execute(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg
) const -> void {
    rhi::CommandLabel label{
        .label = name,
        .color = {0.0f, 0.0f, 1.0f},
    };

    uint32_t rt_width = 0;
    uint32_t rt_height = 0;
    std::vector<rhi::ResourceFormat> color_targets_format;
    rhi::ResourceFormat depth_stencil_format = rhi::ResourceFormat::undefined;

    rhi::RenderTargetDesc rt_desc{};
    rt_desc.colors.reserve(builder.color_targets_.size());
    for (size_t i = 0; i < builder.color_targets_.size(); i++) {
        auto const& target_opt = builder.color_targets_[i];
        if (!target_opt.has_value()) { break; }
        auto const& target = target_opt.value();
        auto texture = rg.texture(target.handle)->rhi_texture();
        if (rt_width == 0) {
            rt_width = texture->desc().extent.width;
            rt_height = texture->desc().extent.height;
        }
        color_targets_format.push_back(texture->desc().format);
        auto& attach_desc = rt_desc.colors.emplace_back(rhi::ColorAttachmentDesc{
            .texture = {
                .texture = texture,
                .mip_level = target.level,
                .base_layer = target.base_layer,
                .num_layers = target.num_layers,
            },
            .clear = target.clear_color.has_value(),
            .store = target.store,
        });
        if (target.clear_color.has_value()) {
            attach_desc.clear_color = {
                target.clear_color.value().x,
                target.clear_color.value().y,
                target.clear_color.value().z,
                target.clear_color.value().w,
            };
        }
    }
    if (builder.depth_stencil_target_.has_value()) {
        auto const& target = builder.depth_stencil_target_.value();
        auto texture = rg.texture(target.handle)->rhi_texture();
        if (rt_width == 0) {
            rt_width = texture->desc().extent.width;
            rt_height = texture->desc().extent.height;
        }
        depth_stencil_format = texture->desc().format;
        rt_desc.depth_stencil = rhi::DepthStencilAttachmentDesc{
            .texture = {
                .texture = texture,
                .mip_level = target.level,
                .base_layer = target.base_layer,
                .num_layers = target.num_layers,
            },
            .clear = target.clear_value.has_value(),
            .store = target.store,
        };
        if (target.clear_value.has_value()) {
            rt_desc.depth_stencil.value().clear_depth = target.clear_value.value().first;
            rt_desc.depth_stencil.value().clear_stencil = target.clear_value.value().second;
        }
    }

    if (rt_width == 0) { return; }

    auto graphics_encoder = cmd_encoder->begin_render_pass(label, rt_desc);

    rhi::Viewport viewport{
        .width = static_cast<float>(rt_width),
        .height = static_cast<float>(rt_height),
    };
    graphics_encoder->set_viewports({viewport});
    rhi::Scissor scissor{
        .width = rt_width,
        .height = rt_height,
    };
    graphics_encoder->set_scissors({scissor});

    GraphicsPassContext context{
        make_cref(rg),
        graphics_encoder.ref(),
        std::move(color_targets_format),
        depth_stencil_format,
    };
    builder.execution_func_(&pass_data, context);
    graphics_encoder.reset();

    // TODO - generate mipmaps for outputs
}

auto RenderGraph::Impl::ComputePassNode::set_barriers(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl &rg
) -> void {
    std::vector<rhi::BufferBarrier> buffer_barriers{};
    std::vector<rhi::TextureBarrier> texture_barriers{};

    get_shader_barriers(
        rg,
        builder.read_buffers_,
        builder.write_buffers_,
        builder.read_textures_,
        builder.write_textures_,
        buffer_barriers,
        texture_barriers
    );

    cmd_encoder->resource_barriers(buffer_barriers, texture_barriers);
}
auto RenderGraph::Impl::ComputePassNode::execute(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg
) const -> void {
    rhi::CommandLabel label{
        .label = name,
        .color = {1.0f, 0.0f, 0.0f},
    };

    auto compute_encoder = cmd_encoder->begin_compute_pass(label);
    ComputePassContext context{
        .rg = make_cref(rg),
        .cmd_encoder = compute_encoder.ref(),
    };
    builder.execution_func_(&pass_data, context);
    compute_encoder.reset();

    // TODO - generate mipmaps for outputs
}

auto RenderGraph::Impl::BlitPassNode::set_barriers(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl &rg
) -> void {
    std::vector<rhi::TextureBarrier> texture_barriers{};

    {
        auto& pool_texture = rg.pool_texture(src);
        auto target_access = rhi::ResourceAccessType::sampled_texture_read;
        if (need_barrier(pool_texture.access, target_access)) {
            texture_barriers.push_back(rhi::TextureBarrier{
                .texture = pool_texture.texture->rhi_texture(),
                .src_access_type = pool_texture.access,
                .dst_access_type = target_access,
            });
            pool_texture.access = target_access;
        }
    }
    {
        auto& pool_texture = rg.pool_texture(dst);
        auto target_access = rhi::is_depth_stencil_format(pool_texture.texture->desc().format)
            ? rhi::ResourceAccessType::depth_stencil_attachment_write
            : rhi::ResourceAccessType::color_attachment_write;
        if (need_barrier(pool_texture.access, target_access)) {
            texture_barriers.push_back(rhi::TextureBarrier{
                .texture = pool_texture.texture->rhi_texture(),
                .src_access_type = pool_texture.access,
                .dst_access_type = target_access,
            });
            pool_texture.access = target_access;
        }
    }

    cmd_encoder->resource_barriers({}, texture_barriers);
}
auto RenderGraph::Impl::BlitPassNode::execute(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph const& rg
) const -> void {
    g_engine->graphics_manager()->blit_texture_2d(
        cmd_encoder,
        rg.texture(src), src_mip_level, src_array_layer,
        rg.texture(dst), dst_mip_level, dst_array_layer
    );
}

auto RenderGraph::Impl::PresentPassNode::set_barriers(
    Ref<rhi::CommandEncoder> cmd_encoder, RenderGraph::Impl &rg
) -> void {
    auto target_access = rhi::ResourceAccessType::sampled_texture_read;
    auto& pool_texture = rg.pool_texture(texture);
    if (need_barrier(pool_texture.access, target_access)) {
        rhi::TextureBarrier barrier{
            .texture = pool_texture.texture->rhi_texture(),
            .src_access_type = pool_texture.access,
            .dst_access_type = target_access,
        };
        pool_texture.access = target_access;
        cmd_encoder->resource_barriers({}, {barrier});
    }
}


RenderGraph::RenderGraph() = default;

RenderGraph::~RenderGraph() = default;

auto RenderGraph::add_buffer(std::function<auto(BufferBuilder&) -> void> setup_func) -> BufferHandle {
    return impl()->add_buffer(std::move(setup_func));
}
auto RenderGraph::import_buffer(Ref<Buffer> buffer) -> BufferHandle {
    return impl()->import_buffer(buffer);
}
auto RenderGraph::add_texture(std::function<auto(TextureBuilder&) -> void> setup_func) -> TextureHandle {
    return impl()->add_texture(std::move(setup_func));
}
auto RenderGraph::import_texture(Ref<Texture> texture, BitFlags<rhi::ResourceAccessType> access) -> TextureHandle {
    return impl()->import_texture(texture, access);
}
auto RenderGraph::import_back_buffer() const -> TextureHandle {
    return impl()->import_back_buffer();
}

auto RenderGraph::execute() -> void {
    impl()->compile();
    impl()->execute(*this);
}

auto RenderGraph::buffer(BufferHandle handle) const -> Ref<Buffer> {
    return impl()->buffer(handle);
}
auto RenderGraph::texture(TextureHandle handle) const -> Ref<Texture> {
    return impl()->texture(handle);
}
auto RenderGraph::rendered_object_list(RenderedObjectListHandle handle) const -> CRef<RenderedObjectList> {
    return impl()->rendered_object_list(handle);
}

auto RenderGraph::add_graphics_pass_impl(
    std::string_view name, std::any&& pass_data
) -> std::pair<GraphicsPassBuilder&, std::any*> {
    return impl()->add_graphics_pass(this, name, std::move(pass_data));
}
auto RenderGraph::add_compute_pass_impl(
    std::string_view name, std::any&& pass_data
) -> std::pair<ComputePassBuilder&, std::any*> {
    return impl()->add_compute_pass(this, name, std::move(pass_data));
}
auto RenderGraph::add_blit_pass(
    std::string_view name,
    TextureHandle src, uint32_t src_mip_level, uint32_t src_array_layer,
    TextureHandle dst, uint32_t dst_mip_level, uint32_t dst_array_layer
) -> void {
    impl()->add_blit_pass(this, name, src, src_mip_level, src_array_layer, dst, dst_mip_level, dst_array_layer);
}

auto RenderGraph::add_rendered_object_list(RenderedObjectListDesc const& desc) -> RenderedObjectListHandle {
    return impl()->add_rendered_object_list(desc);
}

auto RenderGraph::set_graphics_device(Ref<rhi::Device> device, uint32_t num_frames) -> void {
    impl()->set_graphics_device(device, num_frames);
}
auto RenderGraph::set_back_buffer(Ref<Texture> texture, BitFlags<rhi::ResourceAccessType> access) -> void {
    impl()->set_back_buffer(texture, access);
}
auto RenderGraph::set_command_encoder(Ref<rhi::CommandEncoder> cmd_encoder) -> void {
    impl()->set_command_encoder(cmd_encoder);
}

auto RenderGraph::add_read_edge(size_t pass_index, BufferHandle handle) -> void {
    impl()->add_read_edge(pass_index, handle);
}
auto RenderGraph::add_read_edge(size_t pass_index, TextureHandle handle) -> void {
    impl()->add_read_edge(pass_index, handle);
}
auto RenderGraph::add_write_edge(size_t pass_index, BufferHandle handle) -> void {
    impl()->add_write_edge(pass_index, handle);
}
auto RenderGraph::add_write_edge(size_t pass_index, TextureHandle handle) -> void {
    impl()->add_write_edge(pass_index, handle);
}

}

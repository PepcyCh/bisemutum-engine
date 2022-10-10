#pragma once

#include <functional>

#include "resource_pool.hpp"
#include "pass.hpp"
#include "resource.hpp"
#include "helper_pipelines.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class RenderGraph {
public:
    RenderGraph(Ref<gfx::Device> device, Ref<gfx::Queue> queue, uint32_t num_frames = 3);

    BufferHandle AddBuffer(const std::string &name, const std::function<void(BufferBuilder &)> &setup_func);
    BufferHandle ImportBuffer(const std::string &name, Ref<gfx::Buffer> buffer);

    TextureHandle AddTexture(const std::string &name, const std::function<void(TextureBuilder &)> &setup_func);
    TextureHandle ImportTexture(const std::string &name, Ref<gfx::Texture> texture);

    void AddRenderPass(const std::string &name, const std::function<void(RenderPassBuilder &)> &setup_func,
        const std::function<void(Ref<gfx::RenderCommandEncoder>, const PassResource &)> &execute_func);
    void AddComputePass(const std::string &name, const std::function<void(ComputePassBuilder &)> &setup_func,
        const std::function<void(Ref<gfx::ComputeCommandEncoder>, const PassResource &)> &execute_func);
    void AddPresentPass(TextureHandle texture);
    void AddBlitPass(const std::string &name, const std::function<void(BlitPassBuilder &)> &setup_func);

    void Compile();

    void Execute(Span<Ref<gfx::Semaphore>> wait_semaphores = {}, Span<Ref<gfx::Semaphore>> signal_semaphores = {},
        gfx::Fence *signal_fence = nullptr);

private:
    friend PassResource;

    struct Node {
        Vec<Ref<Node>> in_nodes;
        Vec<Ref<Node>> out_nodes;
        std::string name;
        size_t index;

        virtual ~Node() = default;

        virtual bool IsResource() const { return false; }

        virtual void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) {}
        virtual void Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const {}
        virtual void AfterExcution(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) {}

        virtual void Create(RenderGraph &rg) {}
        virtual void Destroy(RenderGraph &rg) {}
    };

    struct BufferNode : Node {
        gfx::BufferDesc desc;
        std::optional<ResourcePool::Buffer> buffer = std::nullopt;
        bool imported = false;

        bool IsResource() const override { return true; }

        void Create(RenderGraph &rg) override;
        void Destroy(RenderGraph &rg) override;
    };
    struct TextureNode : Node {
        gfx::TextureDesc desc;
        std::optional<ResourcePool::Texture> texture = std::nullopt;
        bool imported = false;

        bool IsResource() const override { return true; }

        void Create(RenderGraph &rg) override;
        void Destroy(RenderGraph &rg) override;
    };
    struct RenderPassNode : Node {
        PassResource resource;
        std::optional<RenderPassColorTarget> color_targets[gfx::kMaxRenderTargetsCount];
        std::optional<RenderPassDepthStencilTarget> depth_stencil_target;
        std::function<void(Ref<gfx::RenderCommandEncoder>, const PassResource &)> execute_func;
        size_t num_color_targets;

        void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
        void Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const override;
        void AfterExcution(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
    };
    struct ComputePassNode : Node {
        PassResource resource;
        std::function<void(Ref<gfx::ComputeCommandEncoder>, const PassResource &)> execute_func;

        void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
        void Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const override;
        void AfterExcution(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
    };
    struct PresentPassNode : Node {
        TextureHandle texture;

        void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
    };
    struct BlitPassNode : Node {
        TextureHandle src_handle;
        uint32_t src_level;
        uint32_t src_layer;
        TextureHandle dst_handle;
        uint32_t dst_level;
        uint32_t dst_layer;

        void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
        void Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const override;
    };

    void AddEdge(Ref<Node> from, Ref<Node> to);

    void Clear();

    ResourcePool::Buffer &Buffer(BufferHandle handle);
    const ResourcePool::Buffer &Buffer(BufferHandle handle) const;
    ResourcePool::Texture &Texture(TextureHandle handle);
    const ResourcePool::Texture &Texture(TextureHandle handle) const;

    Vec<Ptr<Node>> graph_nodes_;
    Vec<size_t> graph_order_;
    Vec<Vec<size_t>> resources_to_create_;
    Vec<Vec<size_t>> resources_to_destroy_;
    size_t present_pass_index_ = static_cast<size_t>(-1);

    Ref<gfx::Device> device_;
    Ref<gfx::Queue> queue_;
    Vec<Ptr<gfx::FrameContext>> contexts_;
    Vec<Ptr<ResourcePool>> resource_pools_;
    size_t curr_frame_ = 0;

    Ptr<HelperPipelines> helper_pipelines_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

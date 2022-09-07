#pragma once

#include <functional>

#include "resource_pool.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

class RenderGraph;

struct BufferHandle {
private:
    friend RenderGraph;

    size_t node_index_;
};
struct TextureHandle {
private:
    friend RenderGraph;

    size_t node_index_;
};

enum class BufferReadType : uint8_t {
    eUniform,
    eStorage,
};
class PassResource {
public:
    Ref<gfx::Buffer> Buffer(const std::string &name) const;
    Ref<gfx::Texture> Texture(const std::string &name) const;

private:
    friend RenderGraph;

    void GetShaderBarriers(RenderGraph &rg, gfx::ResourceAccessStage target_access_stage,
        Vec<gfx::BufferBarrier> &buffer_barriers, Vec<gfx::TextureBarrier> &texture_barriers);

    const RenderGraph *graph_;
    HashMap<std::string, BufferHandle> read_buffers_;
    HashMap<std::string, BufferReadType> read_buffers_type_;
    HashMap<std::string, BufferHandle> write_buffers_;
    HashMap<std::string, TextureHandle> read_textures_;
    HashMap<std::string, TextureHandle> write_textures_;
};
struct RenderPassColorTarget {
    TextureHandle handle;
    uint32_t base_layer = 0;
    uint32_t layers = 1;
    uint32_t level = 0;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    } clear_color;
    bool clear = false;
    bool store = true;
};
struct RenderPassDepthStencilTarget {
    TextureHandle handle;
    uint32_t base_layer = 0;
    uint32_t layers = 1;
    uint32_t level = 0;
    float clear_depth = 0.0f;
    uint8_t clear_stencil = 0;
    bool clear = false;
    bool store = true;
};

class RenderGraph {
public:
    RenderGraph(Ref<gfx::Device> device, uint32_t num_frames = 3);

    BufferHandle AddBuffer(const std::string &name, const std::function<void(class BufferBuilder &)> &setup_func);
    BufferHandle ImportBuffer(const std::string &name, Ref<gfx::Buffer> buffer);

    TextureHandle AddTexture(const std::string &name, const std::function<void(class TextureBuilder &)> &setup_func);
    TextureHandle ImportTexture(const std::string &name, Ref<gfx::Texture> texture);

    void AddRenderPass(const std::string &name, const std::function<void(class RenderPassBuilder &)> &setup_func,
        const std::function<void(Ref<gfx::RenderCommandEncoder>, const PassResource &)> &execute_func);
    void AddComputePass(const std::string &name, const std::function<void(class ComputePassBuilder &)> &setup_func,
        const std::function<void(Ref<gfx::ComputeCommandEncoder>, const PassResource &)> &execute_func);

    void Compile();

    void Execute();

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

        void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
        void Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const override;
    };
    struct ComputePassNode : Node {
        PassResource resource;
        std::function<void(Ref<gfx::ComputeCommandEncoder>, const PassResource &)> execute_func;

        void SetBarriers(const Ptr<gfx::CommandEncoder> &cmd_encoder, RenderGraph &rg) override;
        void Execute(const Ptr<gfx::CommandEncoder> &cmd_encoder, const RenderGraph &rg) const override;
    };

    void AddEdge(Ref<Node> from, Ref<Node> to);

    ResourcePool::Buffer &Buffer(BufferHandle handle);
    const ResourcePool::Buffer &Buffer(BufferHandle handle) const;
    ResourcePool::Texture &Texture(TextureHandle handle);
    const ResourcePool::Texture &Texture(TextureHandle handle) const;

    Vec<Ptr<Node>> graph_nodes_;
    Vec<size_t> graph_order_;
    Vec<Vec<size_t>> resources_to_create_;
    Vec<Vec<size_t>> resources_to_destroy_;

    Ref<gfx::Device> device_;
    Vec<Ptr<gfx::FrameContext>> contexts_;
    Vec<Ptr<ResourcePool>> resource_pools_;
    size_t curr_frame_ = 0;
};

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END

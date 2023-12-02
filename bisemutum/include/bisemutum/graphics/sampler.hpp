#pragma once

#include "../prelude/box.hpp"
#include "../rhi/descriptor.hpp"

namespace bi::gfx {

struct GraphicsManager;

struct Sampler final {
    Sampler() = default;
    ~Sampler();

    auto initialize(rhi::SamplerDesc const& desc) -> void;

    auto has_value() const -> bool;
    auto reset() -> void;

    auto rhi_sampler() -> Ref<rhi::Sampler> { return sampler_.ref(); }
    auto rhi_sampler() const -> CRef<rhi::Sampler> { return sampler_.ref(); }

    auto get_descriptor() const -> rhi::DescriptorHandle { return cpu_descriptor_; }

private:
    Box<rhi::Sampler> sampler_;
    rhi::DescriptorHandle cpu_descriptor_;
};

}

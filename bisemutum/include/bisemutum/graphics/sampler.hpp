#pragma once

#include <unordered_map>

#include "../prelude/box.hpp"
#include "../rhi/descriptor.hpp"

namespace bi::gfx {

struct GraphicsManager;

struct Sampler final {
    Sampler() = default;

    auto initialize(rhi::SamplerDesc const& desc) -> void;

    auto has_value() const -> bool;
    auto reset() -> void;

    auto rhi_sampler() -> Ref<rhi::Sampler>;
    auto rhi_sampler() const -> CRef<rhi::Sampler>;

    auto get_descriptor() const -> rhi::DescriptorHandle { return cpu_descriptor_; }

private:
    Box<rhi::Sampler> sampler_;
    rhi::DescriptorHandle cpu_descriptor_;
};

}

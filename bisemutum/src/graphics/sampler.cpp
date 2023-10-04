#include <bisemutum/graphics/sampler.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi::gfx {

auto Sampler::initialize(rhi::SamplerDesc const& desc) -> void {
    if (sampler_) { return; }

    sampler_ = g_engine->graphics_manager()->device()->create_sampler(desc);

    cpu_descriptor_ = g_engine->graphics_manager()->allocate_cpu_descriptor(rhi::DescriptorType::sampler);
    g_engine->graphics_manager()->device()->create_descriptor(sampler_.ref(), cpu_descriptor_);
}

Sampler::~Sampler() {
    reset();
}

auto Sampler::has_value() const -> bool {
    return sampler_;
}

auto Sampler::reset() -> void {
    sampler_.reset();

    if (cpu_descriptor_.cpu != 0) {
        g_engine->graphics_manager()->free_cpu_sampler_descriptor(cpu_descriptor_);
        cpu_descriptor_ = {};
    }
}

}

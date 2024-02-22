#include <bisemutum/graphics/accel.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi::gfx {

AccelerationStructure::AccelerationStructure(AccelerationStructureDesc const& desc) {
    // TODO
}

AccelerationStructure::~AccelerationStructure() {
    reset();
}

AccelerationStructure::AccelerationStructure(AccelerationStructure&& rhs) noexcept
    : tlas_(std::move(rhs.tlas_))
    , cpu_descriptor_(rhs.cpu_descriptor_)
{}

auto AccelerationStructure::operator=(AccelerationStructure&& rhs) noexcept -> AccelerationStructure& {
    reset();
    tlas_ = std::move(rhs.tlas_);
    cpu_descriptor_ = rhs.cpu_descriptor_;
    return *this;
}

auto AccelerationStructure::has_value() const -> bool {
    return tlas_;
}
auto AccelerationStructure::reset() -> void {
    if (has_value()) {
        g_engine->graphics_manager()->add_delayed_destroy(
            [
                tlas = std::move(tlas_),
                cpu_descriptor = cpu_descriptor_
            ]() {
                g_engine->graphics_manager()->free_cpu_resource_descriptor(cpu_descriptor);
            }
        );
    }
}

auto AccelerationStructure::get_descriptor() -> rhi::DescriptorHandle {
    // TODO
    return {};
}

}

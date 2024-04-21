#pragma once

#include "../rhi/accel.hpp"
#include "drawable.hpp"

namespace bi::gfx {

struct AccelerationStructureDesc final {
    Ptr<GpuSceneSystem> gpu_scene;
    std::vector<Ref<Drawable>> drawables;
};

struct AccelerationStructure final {
    AccelerationStructure() = default;
    AccelerationStructure(AccelerationStructureDesc const& desc);
    ~AccelerationStructure();

    AccelerationStructure(AccelerationStructure&& rhs) noexcept;
    auto operator=(AccelerationStructure&& rhs) noexcept -> AccelerationStructure&;

    auto has_value() const -> bool;
    auto reset() -> void;

    auto get_descriptor() -> rhi::DescriptorHandle;

private:
    auto create_buffer(uint32_t size) -> void;

    Box<rhi::AccelerationStructure> tlas_;
    Buffer tlas_buffer_;
    rhi::DescriptorHandle cpu_descriptor_{};
};

struct GeometryAccelerationStructure final {
private:
    friend AccelerationStructure;

    auto create_buffer(uint32_t size) -> void;
    auto compact_buffer(uint32_t size) -> Box<rhi::AccelerationStructure>;

    Box<rhi::AccelerationStructure> blas_;
    Buffer blas_buffer_;
};

}

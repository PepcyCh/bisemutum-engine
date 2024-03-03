#pragma once

#include "../rhi/accel.hpp"
#include "drawable.hpp"

namespace bi::gfx {

struct AccelerationStructureInstance final {
    Ptr<Drawable> drawable = nullptr;
    Ptr<Buffer> aabb_buffer = nullptr;
    uint32_t aabb_buffer_offset = 0;
    uint32_t aabb_num_primitives = 0;

    uint32_t instance_id = 0;
    uint32_t instance_mask = 0xff;
};

struct AccelerationStructureDesc final {
    std::vector<AccelerationStructureInstance> instances;
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

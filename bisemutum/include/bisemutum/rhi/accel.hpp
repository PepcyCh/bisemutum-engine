#pragma once

#include <variant>

#include "../prelude/ref.hpp"
#include "../prelude/span.hpp"
#include "resource.hpp"

namespace bi::rhi {

struct AccelerationStructure;

struct AccelerationStructureTriangleDesc final {
    ResourceFormat vertex_format = ResourceFormat::rgb32_sfloat;
    IndexType index_type = IndexType::uint32;
    uint32_t num_vertices = 0;
    uint32_t num_triangles = 0;
    uint32_t vertex_stride = 0;
    CRef<Buffer> vertex_buffer;
    CRef<Buffer> index_buffer;
    uint64_t vertex_buffer_offset = 0;
    uint64_t index_buffer_offset = 0;
};
struct AccelerationStructureProcedualDesc final {
    uint32_t num_primitives = 0;
    uint32_t aabb_stride = 6 * sizeof(float);
    CRef<Buffer> aabb_buffer;
    uint64_t aabb_buffer_offset = 0;
};
enum class AccelerationStructureGeometryFlag : uint8_t {
    none = 0,
    opaque = 1,
    no_duplicate_any_hit = 2,
};
struct AccelerationStructureGeometryDesc final {
    BitFlags<AccelerationStructureGeometryFlag> flags = {};
    std::variant<AccelerationStructureTriangleDesc, AccelerationStructureProcedualDesc> geometry;
};

enum class AccelerationStructureInstanceFlag : uint8_t {
    none = 0,
    triangle_cull_disable = 1,
    triangle_front_face_ccw = 2,
    force_opaque = 4,
    force_non_opaque = 8,
};
// This structure must be the same as that in Vulkan or D3D12.
struct AccelerationStructureInstanceDesc final {
    float transform[3][4];
    uint32_t instance_id : 24 = 0;
    uint32_t mask : 8 = 0xff;
    uint32_t sbt_offset : 24 = 0;
    uint32_t flags : 8 = 0;
    uint64_t blas = 0;
};

struct AccelerationStructureMemoryInfo final {
    uint64_t acceleration_structure_size = 0;
    uint64_t build_scratch_size = 0;
    uint64_t update_scratch_size = 0;
};

enum class AccelerationStructureBuildFlag : uint8_t {
    none = 0,
    allow_update = 1,
    allow_compaction = 2,
    fast_trace = 4,
    fast_build = 8,
    minimize_memory = 16,
};

enum class AccelerationStructureBuildEmitDataType : uint8_t {
    compacted_size,
};
struct AccelerationStructureBuildEmitData final {
    AccelerationStructureBuildEmitDataType type = AccelerationStructureBuildEmitDataType::compacted_size;
    Ref<Buffer> dst_buffer;
    uint64_t dst_buffer_offset = 0;
};

struct AccelerationStructureGeometryBuildInput final {
    BitFlags<AccelerationStructureGeometryFlag> flags = {};
    bool is_update = false;
    CSpan<AccelerationStructureGeometryDesc> geometries;
};
struct AccelerationStructureGeometryBuildDesc final {
    AccelerationStructureGeometryBuildInput build_input;
    CRef<Buffer> scratch_buffer;
    uint64_t scratch_buffer_offset = 0;
    CPtr<AccelerationStructure> src_acceleration_structure = nullptr;
    Ref<AccelerationStructure> dst_acceleration_structure;
    Span<AccelerationStructureBuildEmitData> emit_data;
};
struct AccelerationStructureInstanceBuildInput final {
    BitFlags<AccelerationStructureGeometryFlag> flags = {};
    bool is_update = false;
    uint32_t num_instances = 0;
    CRef<Buffer> instances_buffer;
    uint64_t instances_buffer_offset = 0;
};
struct AccelerationStructureInstanceBuildDesc final {
    AccelerationStructureInstanceBuildInput build_input;
    CRef<Buffer> scratch_buffer;
    uint64_t scratch_buffer_offset = 0;
    CPtr<AccelerationStructure> src_acceleration_structure = nullptr;
    Ref<AccelerationStructure> dst_acceleration_structure;
    Span<AccelerationStructureBuildEmitData> emit_data;
};

enum class AccelerationStructureType : uint8_t {
    bottom_level,
    top_level,
};
struct AccelerationStructureDesc final {
    AccelerationStructureType type = AccelerationStructureType::bottom_level;
    Ptr<Buffer> buffer;
    uint64_t buffer_offset = 0;
    uint64_t buffer_range_size = 0;
};

struct AccelerationStructure {
    virtual ~AccelerationStructure() = default;

    auto desc() const -> AccelerationStructureDesc const& { return desc_; }

    auto base_buffer() -> Ref<Buffer> { return desc_.buffer.value(); }
    auto base_buffer() const -> CRef<Buffer> { return desc_.buffer.value(); }

    virtual auto gpu_reference() const -> uint64_t = 0;

protected:
    AccelerationStructureDesc desc_;
};

}

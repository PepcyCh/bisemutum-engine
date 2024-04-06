#include <bisemutum/graphics/accel.hpp>

#include <bisemutum/prelude/hash.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/rhi/device.hpp>

namespace bi::gfx {

AccelerationStructure::AccelerationStructure(AccelerationStructureDesc const& desc) {
    std::unordered_set<CRef<Drawable>> drawables;
    for (auto& drawable : desc.drawables) {
        if (drawable->submesh_desc().topology == rhi::PrimitiveTopology::triangle_list) {
            drawables.insert(drawable);
        }
    }

    // Build BLASes

    std::vector<rhi::AccelerationStructureGeometryBuildInput> blas_build_inputs;
    std::vector<Ref<GeometryAccelerationStructure>> blas_to_be_built;
    std::vector<uint32_t> blas_scratch_buffer_offsets;
    uint32_t blas_scratch_buffer_size = 0;
    std::unordered_map<CRef<Drawable>, Ref<GeometryAccelerationStructure>> drawables_blas;
    for (auto drawable : drawables) {
        auto [build_input_opt, blas] = g_engine->graphics_manager()->require_blas_build_desc(drawable);
        if (build_input_opt) {
            auto& build_input = build_input_opt.value();
            auto size_info = g_engine->graphics_manager()->device()->get_acceleration_structure_memory_size(build_input);
            blas_scratch_buffer_offsets.push_back(blas_scratch_buffer_size);
            if (build_input.is_update) {
                blas_scratch_buffer_size += size_info.update_scratch_size;
            } else {
                blas_scratch_buffer_size += size_info.build_scratch_size;
                blas->create_buffer(size_info.acceleration_structure_size);
            }
            blas_build_inputs.push_back(std::move(build_input_opt).value());
            blas_to_be_built.push_back(blas);
        }
        drawables_blas.insert({drawable, blas});
    }
    if (!blas_to_be_built.empty()) {
        Buffer scratch_buffer(rhi::BufferDesc{
            .size = blas_scratch_buffer_size + 256,
            .usages = rhi::BufferUsage::storage_read_write,
        }, false);
        Buffer emit_buffer(rhi::BufferDesc{
            .size = blas_to_be_built.size() * sizeof(uint64_t),
            .usages = rhi::BufferUsage::storage_read_write,
        }, false);
        Buffer emit_download_buffer(rhi::BufferDesc{
            .size = blas_to_be_built.size() * sizeof(uint64_t),
            .memory_property = rhi::BufferMemoryProperty::gpu_to_cpu,
        });
        uint32_t emit_i = 0;
        std::vector<rhi::AccelerationStructureGeometryBuildDesc> blas_build_descs;
        blas_build_descs.reserve(blas_to_be_built.size());
        for (size_t i = 0; i < blas_to_be_built.size(); i++) {
            blas_build_descs.push_back(rhi::AccelerationStructureGeometryBuildDesc{
                .build_input = std::move(blas_build_inputs[i]),
                .scratch_buffer = scratch_buffer.rhi_buffer(),
                .scratch_buffer_offset = blas_scratch_buffer_offsets[i],
                .dst_acceleration_structure = blas_to_be_built[i]->blas_.ref(),
            });
            if (blas_build_descs.back().build_input.is_update) {
                blas_build_descs.back().src_acceleration_structure = blas_build_descs.back().dst_acceleration_structure;
            } else {
                blas_build_descs.back().emit_data.push_back(rhi::AccelerationStructureBuildEmitData{
                    .type = rhi::AccelerationStructureBuildEmitDataType::compacted_size,
                    .dst_buffer = emit_buffer.rhi_buffer(),
                    .dst_buffer_offset = emit_i * sizeof(uint64_t),
                });
                ++emit_i;
            }
        }
        g_engine->graphics_manager()->execute_immediately([&](Ref<rhi::CommandEncoder> cmd) {
            cmd->build_bottom_level_acceleration_structure(blas_build_descs);
            cmd->resource_barriers({
                rhi::BufferBarrier{
                    .buffer = emit_buffer.rhi_buffer(),
                    .src_access_type = rhi::ResourceAccessType::acceleration_structure_build_emit_data_write,
                    .dst_access_type = rhi::ResourceAccessType::transfer_read,
                }
            }, {});
            cmd->copy_buffer_to_buffer(emit_buffer.rhi_buffer(), emit_download_buffer.rhi_buffer(), {});
        });
        if (emit_i > 0) {
            std::vector<uint64_t> compacted_sizes(emit_i);
            emit_download_buffer.get_data_raw(compacted_sizes.data(), compacted_sizes.size() * sizeof(uint64_t));
            emit_i = 0;
            for (size_t i = 0; i < blas_to_be_built.size(); i++, emit_i++) {
                if (blas_build_descs[i].build_input.is_update) { continue; }
                if (compacted_sizes[emit_i] < blas_to_be_built[i]->blas_buffer_.desc().size) {
                    auto original_blas = blas_to_be_built[i]->compact_buffer(compacted_sizes[emit_i]);
                    g_engine->graphics_manager()->execute_immediately([&](Ref<rhi::CommandEncoder> cmd) {
                        cmd->compact_acceleration_structure(original_blas.ref(), blas_to_be_built[i]->blas_.ref());
                    });
                }
            }
        }
    }

    std::vector<rhi::AccelerationStructureInstanceDesc> tlas_instances;
    for (auto& drawable : desc.drawables) {
        auto& blas = drawables_blas.at(drawable);
        tlas_instances.push_back(rhi::AccelerationStructureInstanceDesc{
            .instance_id = static_cast<uint32_t>(drawable->handle()),
            .mask = 0xff,
            .sbt_offset = static_cast<uint32_t>(drawable->handle()),
            .flags = BitFlags{
                drawable->material->blend_mode == BlendMode::opaque
                    ? rhi::AccelerationStructureInstanceFlag::force_opaque
                    : rhi::AccelerationStructureInstanceFlag::force_non_opaque
                }.raw_value(),
            .blas = blas->blas_->gpu_reference(),
        });
        auto transform = drawable->transform.matrix();
        tlas_instances.back().transform[0][0] = transform[0][0];
        tlas_instances.back().transform[0][1] = transform[1][0];
        tlas_instances.back().transform[0][2] = transform[2][0];
        tlas_instances.back().transform[0][3] = transform[3][0];
        tlas_instances.back().transform[1][0] = transform[0][1];
        tlas_instances.back().transform[1][1] = transform[1][1];
        tlas_instances.back().transform[1][2] = transform[2][1];
        tlas_instances.back().transform[1][3] = transform[3][1];
        tlas_instances.back().transform[2][0] = transform[0][2];
        tlas_instances.back().transform[2][1] = transform[1][2];
        tlas_instances.back().transform[2][2] = transform[2][2];
        tlas_instances.back().transform[2][3] = transform[3][2];
    }
    Buffer tlas_instance_buffer(rhi::BufferDesc{
        .size = tlas_instances.size() * sizeof(rhi::AccelerationStructureInstanceDesc),
        .usages = rhi::BufferUsage::acceleration_structure_build,
    });
    tlas_instance_buffer.set_data_immediately(tlas_instances.data(), tlas_instances.size());
    rhi::AccelerationStructureInstanceBuildInput tlas_build_input{
        .flags = rhi::AccelerationStructureBuildFlag::fast_trace,
        .is_update = false,
        .num_instances = static_cast<uint32_t>(tlas_instances.size()),
        .instances_buffer = tlas_instance_buffer.rhi_buffer(),
    };
    auto tlas_size_info = g_engine->graphics_manager()->device()->get_acceleration_structure_memory_size(tlas_build_input);
    Buffer tlas_scratch_buffer(rhi::BufferDesc{
        .size = tlas_size_info.build_scratch_size + 256,
        .usages = rhi::BufferUsage::storage_read_write,
    }, false);
    create_buffer(tlas_size_info.acceleration_structure_size);
    rhi::AccelerationStructureInstanceBuildDesc tlas_build_desc{
        .build_input = std::move(tlas_build_input),
        .scratch_buffer = tlas_scratch_buffer.rhi_buffer(),
        .dst_acceleration_structure = tlas_.ref(),
    };
    g_engine->graphics_manager()->execute_immediately([&](Ref<rhi::CommandEncoder> cmd) {
        cmd->build_top_level_acceleration_structure(tlas_build_desc);
    });
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
                tlas_buffer = std::move(tlas_buffer_),
                cpu_descriptor = cpu_descriptor_
            ]() {
                g_engine->graphics_manager()->free_cpu_resource_descriptor(cpu_descriptor);
            }
        );
    }
}

auto AccelerationStructure::get_descriptor() -> rhi::DescriptorHandle {
    if (cpu_descriptor_.cpu == 0) {
        cpu_descriptor_ = g_engine->graphics_manager()->allocate_cpu_descriptor(rhi::DescriptorType::acceleration_structure);
        g_engine->graphics_manager()->device()->create_descriptor(tlas_.ref(), cpu_descriptor_);
    }
    return cpu_descriptor_;
}

auto AccelerationStructure::create_buffer(uint32_t size) -> void {
    tlas_buffer_ = Buffer(rhi::BufferDesc{
        .size = size,
        .usages = rhi::BufferUsage::acceleration_structure,
    }, false);
    tlas_ = g_engine->graphics_manager()->device()->create_acceleration_structure(rhi::AccelerationStructureDesc{
        .type = rhi::AccelerationStructureType::top_level,
        .buffer = tlas_buffer_.rhi_buffer(),
        .buffer_offset = 0,
        .buffer_range_size = size,
    });
}


auto GeometryAccelerationStructure::create_buffer(uint32_t size) -> void {
    blas_buffer_ = Buffer(rhi::BufferDesc{
        .size = size,
        .usages = rhi::BufferUsage::acceleration_structure,
    }, false);
    blas_ = g_engine->graphics_manager()->device()->create_acceleration_structure(rhi::AccelerationStructureDesc{
        .type = rhi::AccelerationStructureType::bottom_level,
        .buffer = blas_buffer_.rhi_buffer(),
        .buffer_offset = 0,
        .buffer_range_size = size,
    });
}

auto GeometryAccelerationStructure::compact_buffer(uint32_t size) -> Box<rhi::AccelerationStructure> {
    blas_buffer_ = Buffer(rhi::BufferDesc{
        .size = size,
        .usages = rhi::BufferUsage::acceleration_structure,
    }, false);
    auto temp_blas = g_engine->graphics_manager()->device()->create_acceleration_structure(rhi::AccelerationStructureDesc{
        .type = rhi::AccelerationStructureType::bottom_level,
        .buffer = blas_buffer_.rhi_buffer(),
        .buffer_offset = 0,
        .buffer_range_size = size,
    });
    std::swap(blas_, temp_blas);
    return temp_blas;
}

}

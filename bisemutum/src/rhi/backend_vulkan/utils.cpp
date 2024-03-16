#include "utils.hpp"

#include "resource.hpp"

namespace bi::rhi {

auto to_vk_accel_build_info(
    AccelerationStructureGeometryBuildInput const& build_info,
    VkAccelerationStructureBuildGeometryInfoKHR& vk_build_info,
    std::vector<VkAccelerationStructureGeometryKHR>& vk_geometries,
    std::vector<VkAccelerationStructureBuildRangeInfoKHR>& vk_range_infos
) -> void {
    vk_geometries.resize(build_info.geometries.size());
    vk_range_infos.resize(build_info.geometries.size());
    for (size_t i = 0; auto& geo : build_info.geometries) {
        vk_geometries[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        vk_geometries[i].pNext = nullptr;
        vk_geometries[i].flags = geo.flags.raw_value();

        if (auto triangle = std::get_if<AccelerationStructureTriangleDesc>(&geo.geometry); triangle) {
            vk_geometries[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

            vk_geometries[i].geometry.triangles.sType =
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            vk_geometries[i].geometry.triangles.pNext = nullptr;
            vk_geometries[i].geometry.triangles.vertexFormat = to_vk_format(triangle->vertex_format);
            vk_geometries[i].geometry.triangles.vertexData.deviceAddress =
                triangle->vertex_buffer.cast_to<const BufferVulkan>()->address() + triangle->vertex_buffer_offset;
            vk_geometries[i].geometry.triangles.vertexStride = triangle->vertex_stride;
            vk_geometries[i].geometry.triangles.maxVertex = triangle->num_vertices;
            vk_geometries[i].geometry.triangles.indexType = to_vk_index_type(triangle->index_type);
            vk_geometries[i].geometry.triangles.indexData.deviceAddress =
                triangle->index_buffer.cast_to<const BufferVulkan>()->address() + triangle->index_buffer_offset;
            vk_geometries[i].geometry.triangles.transformData.deviceAddress = 0;

            vk_range_infos[i].primitiveCount = triangle->num_triangles;
            vk_range_infos[i].primitiveOffset = 0;
            vk_range_infos[i].firstVertex = 0;
            vk_range_infos[i].transformOffset = 0;
        } else {
            auto& aabb = std::get<AccelerationStructureProcedualDesc>(geo.geometry);

            vk_geometries[i].geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;

            vk_geometries[i].geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
            vk_geometries[i].geometry.aabbs.pNext = nullptr;
            vk_geometries[i].geometry.aabbs.data.deviceAddress =
                aabb.aabb_buffer.cast_to<const BufferVulkan>()->address() + aabb.aabb_buffer_offset;
            vk_geometries[i].geometry.aabbs.stride = aabb.aabb_stride;

            vk_range_infos[i].primitiveCount = aabb.num_primitives;
            vk_range_infos[i].primitiveOffset = 0;
            vk_range_infos[i].firstVertex = 0;
            vk_range_infos[i].transformOffset = 0;
        }

        ++i;
    }

    vk_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    vk_build_info.pNext = nullptr;
    vk_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    vk_build_info.flags = build_info.flags.raw_value();
    vk_build_info.mode = build_info.is_update
        ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
        : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    vk_build_info.geometryCount = static_cast<uint32_t>(vk_geometries.size());
    vk_build_info.pGeometries = vk_geometries.data();
    vk_build_info.ppGeometries = nullptr;
    vk_build_info.srcAccelerationStructure = VK_NULL_HANDLE;
    vk_build_info.dstAccelerationStructure = VK_NULL_HANDLE;
}

auto to_vk_accel_build_info(
    AccelerationStructureInstanceBuildInput const& build_info,
    VkAccelerationStructureBuildGeometryInfoKHR& vk_build_info,
    VkAccelerationStructureGeometryKHR& vk_geometry,
    VkAccelerationStructureBuildRangeInfoKHR& vk_range_info
) -> void {
    vk_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    vk_geometry.pNext = nullptr;
    vk_geometry.flags = 0;
    vk_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    vk_geometry.geometry = {
        .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .pNext = nullptr,
            .arrayOfPointers = VK_FALSE,
            .data = {
                .deviceAddress = build_info.instances_buffer.cast_to<const BufferVulkan>()->address()
                    + build_info.instances_buffer_offset,
            },
        },
    };

    vk_range_info.primitiveCount = build_info.num_instances;
    vk_range_info.primitiveOffset = 0;
    vk_range_info.firstVertex = 0;
    vk_range_info.transformOffset = 0;

    vk_build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    vk_build_info.pNext = nullptr;
    vk_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    vk_build_info.flags = build_info.flags.raw_value();
    vk_build_info.mode = build_info.is_update
        ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
        : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    vk_build_info.geometryCount = 1;
    vk_build_info.pGeometries = &vk_geometry;
    vk_build_info.ppGeometries = nullptr;
    vk_build_info.srcAccelerationStructure = VK_NULL_HANDLE;
    vk_build_info.dstAccelerationStructure = VK_NULL_HANDLE;
}

}

#include "utils.hpp"

#include "resource.hpp"

namespace bi::rhi {

auto to_dx_accel_build_input(
    AccelerationStructureGeometryBuildInput const& build_info,
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& dx_build_info,
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& dx_geometries
) -> void {
    dx_geometries.resize(build_info.geometries.size());
    for (size_t i = 0; auto& geo : build_info.geometries) {
        dx_geometries[i].Flags = static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(geo.flags.raw_value());

        if (auto triangle = std::get_if<AccelerationStructureTriangleDesc>(&geo.geometry); triangle) {
            dx_geometries[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

            dx_geometries[i].Triangles.Transform3x4 = 0;
            dx_geometries[i].Triangles.VertexFormat = to_dx_format(triangle->vertex_format);
            dx_geometries[i].Triangles.VertexBuffer = {
                .StartAddress = triangle->vertex_buffer.cast_to<const BufferD3D12>()->raw()->GetGPUVirtualAddress()
                    + triangle->vertex_buffer_offset,
                .StrideInBytes = triangle->vertex_stride,
            };
            dx_geometries[i].Triangles.VertexCount = triangle->num_vertices;
            dx_geometries[i].Triangles.IndexFormat = to_dx_index_format(triangle->index_type);
            dx_geometries[i].Triangles.IndexBuffer =
                triangle->index_buffer.cast_to<const BufferD3D12>()->raw()->GetGPUVirtualAddress()
                    + triangle->index_buffer_offset;
            dx_geometries[i].Triangles.IndexCount = triangle->num_triangles * 3;
        } else {
            auto& aabb = std::get<AccelerationStructureProcedualDesc>(geo.geometry);

            dx_geometries[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;

            dx_geometries[i].AABBs.AABBs = {
                .StartAddress = aabb.aabb_buffer.cast_to<const BufferD3D12>()->raw()->GetGPUVirtualAddress()
                    + aabb.aabb_buffer_offset,
                .StrideInBytes = aabb.aabb_stride,
            };
            dx_geometries[i].AABBs.AABBCount = aabb.num_primitives;
        }

        ++i;
    }

    dx_build_info.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    dx_build_info.Flags = static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(build_info.flags.raw_value());
    dx_build_info.NumDescs = static_cast<uint32_t>(build_info.geometries.size());
    dx_build_info.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    dx_build_info.pGeometryDescs = dx_geometries.data();
}

auto to_dx_accel_build_input(
    AccelerationStructureInstanceBuildInput const& build_info,
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& dx_build_info
) -> void {
    dx_build_info.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    dx_build_info.Flags = static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(build_info.flags.raw_value());
    dx_build_info.NumDescs = build_info.num_instances;
    dx_build_info.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    dx_build_info.InstanceDescs = build_info.instances_buffer.cast_to<const BufferD3D12>()->raw()->GetGPUVirtualAddress()
        + build_info.instances_buffer_offset;
}

}

#pragma once

#include "vertex_attributes_type.hpp"
#include "shader_param.hpp"
#include "shader_compilation_environment.hpp"
#include "../prelude/poly.hpp"
#include "../math/bbox.hpp"
#include "../rhi/pipeline.hpp"
#include "../rhi/command.hpp"

namespace bi::gfx {

struct Drawable;

BI_TRAIT_BEGIN(IMesh, move)
    template <typename T>
    static auto helper_primitive_topology(T const& self) -> rhi::PrimitiveTopology {
        return rhi::PrimitiveTopology::triangle_list;
    }
    template <typename T> requires requires (const T v) { v.primitive_topology(); }
    static auto helper_primitive_topology(T const& self) -> rhi::PrimitiveTopology {
        return self.primitive_topology();
    }

    template <typename T>
    static auto helper_tessellation_desc(T const& self) -> rhi::TessellationState { return {}; }
    template <typename T> requires requires (const T v) { v.tessellation_desc(); }
    static auto helper_tessellation_desc(T const& self) -> rhi::TessellationState {
        return self.tessellation_desc();
    }

    template <typename T>
    static auto helper_modify_compiler_environment(
        T const& self, ShaderCompilationEnvironment& compilation_environment
    ) -> void {}
    template <typename T> requires requires (const T v) { v.modify_compiler_environment(); }
    static auto helper_modify_compiler_environment(
        T const& self, ShaderCompilationEnvironment& compilation_environment
    ) -> void {
        return self.modify_compiler_environment();
    }

    BI_TRAIT_METHOD(mesh_type_name, (const& self) requires (self.mesh_type_name()) -> std::string_view)

    BI_TRAIT_METHOD(bounding_box, (const& self) requires (self.bounding_box()) -> BoundingBox)

    BI_TRAIT_METHOD(primitive_topology,
        (const& self) requires (helper_primitive_topology(self)) -> rhi::PrimitiveTopology
    )
    BI_TRAIT_METHOD(vertex_input_desc,
        (const& self, VertexAttributesType attributes_type)
            requires (self.vertex_input_desc(attributes_type)) -> std::vector<rhi::VertexInputBufferDesc>
    )
    BI_TRAIT_METHOD(tessellation_desc,
        (const& self) requires (helper_tessellation_desc(self)) -> rhi::TessellationState
    )
    BI_TRAIT_METHOD(num_indices, (const& self) requires (self.num_indices()) -> uint32_t)
    BI_TRAIT_METHOD(bind_buffers,
        (&self, Ref<rhi::GraphicsCommandEncoder> cmd_encoder) requires (self.bind_buffers(cmd_encoder)) -> void
    )

    BI_TRAIT_METHOD(fill_shader_params,
        (const& self, Ref<Drawable> drawable) requires (self.fill_shader_params(drawable)) -> void
    )
    BI_TRAIT_METHOD(shader_params_metadata,
        (const& self) requires (self.shader_params_metadata()) -> ShaderParameterMetadataList const&
    )

    BI_TRAIT_METHOD(source_path,
        (const& self, rhi::ShaderStage stage) requires (self.source_path(stage)) -> std::string_view
    )
    BI_TRAIT_METHOD(source_entry,
        (const& self, rhi::ShaderStage stage) requires (self.source_entry(stage)) -> std::string_view
    )
    BI_TRAIT_METHOD(modify_compiler_environment,
        (const& self, ShaderCompilationEnvironment& compilation_environment)
            requires (helper_modify_compiler_environment(self, compilation_environment)) -> void
    )
BI_TRAIT_END(IMesh)

}

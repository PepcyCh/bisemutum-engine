#pragma once

#include <bisemutum/prelude/bitflags.hpp>
#include <bisemutum/rhi/shader.hpp>

namespace bi::gfx {

inline constexpr uint32_t graphics_set_mesh = 0;
inline constexpr uint32_t graphics_set_material = 1;
inline constexpr uint32_t graphics_set_fragment = 2;
inline constexpr uint32_t graphics_set_camera = 3;
inline constexpr uint32_t graphics_set_samplers = 4;

inline constexpr uint32_t compute_set_normal = 0;
inline constexpr uint32_t compute_set_camera = 1;
inline constexpr uint32_t compute_set_samplers = 2;

inline constexpr uint32_t raytracing_set_normal = 0;
inline constexpr uint32_t raytracing_set_camera = 1;
inline constexpr uint32_t raytracing_set_scene = 2;
inline constexpr uint32_t raytracing_set_samplers = 3;

inline constexpr uint32_t samplers_binding_shift = 32;

inline constexpr uint32_t possible_max_sets = 8;

inline constexpr BitFlags<rhi::ShaderStage> graphics_set_visibility_mesh =
    BitFlags{rhi::ShaderStage::all_graphics}.clear(rhi::ShaderStage::fragment);
inline constexpr BitFlags<rhi::ShaderStage> graphics_set_visibility_material = rhi::ShaderStage::fragment;
inline constexpr BitFlags<rhi::ShaderStage> graphics_set_visibility_fragment = rhi::ShaderStage::fragment;
inline constexpr BitFlags<rhi::ShaderStage> graphics_set_visibility_camera = rhi::ShaderStage::all_graphics;
inline constexpr BitFlags<rhi::ShaderStage> graphics_set_visibility_samplers = rhi::ShaderStage::all_graphics;
inline constexpr BitFlags<rhi::ShaderStage> graphics_sets_visibility[]{
    graphics_set_visibility_mesh,
    graphics_set_visibility_material,
    graphics_set_visibility_fragment,
    graphics_set_visibility_camera,
    graphics_set_visibility_samplers,
};

inline constexpr BitFlags<rhi::ShaderStage> raytracing_set_visibility_scene =
    BitFlags{rhi::ShaderStage::all_ray_tracing}.clear(rhi::ShaderStage::ray_generation);

}

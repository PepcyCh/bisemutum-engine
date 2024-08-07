#include "register.hpp"

#include <bisemutum/runtime/system_manager.hpp>

#include <bisemutum/runtime/transform_system.hpp>
#include <bisemutum/runtime/prefab_manager.hpp>

#include <bisemutum/graphics/gpu_scene_system.hpp>

#include <bisemutum/scene_basic/camera_system.hpp>
#include <bisemutum/scene_basic/static_mesh_render_system.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi {

auto register_systems(Ref<rt::SystemManager> mgr) -> void {
    mgr->register_system<rt::TransformSystem>();
    mgr->register_global_system<rt::PrefabManager>();

    mgr->register_system<gfx::GpuSceneSystem>();

    mgr->register_system<CameraSystem>();
    mgr->register_system<StaticMeshRenderSystem>();
    mgr->register_system<SkyboxSystem>();
}

}

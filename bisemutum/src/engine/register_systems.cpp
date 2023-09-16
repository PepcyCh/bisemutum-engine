#include "register.hpp"

#include <bisemutum/runtime/ecs.hpp>

#include <bisemutum/runtime/transform_system.hpp>

#include <bisemutum/scene_basic/camera_system.hpp>
#include <bisemutum/scene_basic/static_mesh_render_system.hpp>

namespace bi {

auto register_systems(Ref<rt::EcsManager> mgr) -> void {
    mgr->register_system<rt::TransformSystem>();

    mgr->register_system<CameraSystem>();
    mgr->register_system<StaticMeshRenderSystem>();
}

}

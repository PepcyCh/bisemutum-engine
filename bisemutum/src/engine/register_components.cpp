#include "register.hpp"

#include <bisemutum/runtime/component_manager.hpp>

#include <bisemutum/math/transform.hpp>

#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/scene_basic/light.hpp>
#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/mesh_renderer.hpp>

namespace bi {

auto register_components(Ref<rt::ComponentManager> mgr) -> void {
    mgr->register_component<Transform>();

    mgr->register_component<CameraComponent>();
    mgr->register_component<LightComponent>();
    mgr->register_component<StaticMeshComponent>();
    mgr->register_component<MeshRendererComponent>();
}

}

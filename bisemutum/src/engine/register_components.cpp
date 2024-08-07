#include "register.hpp"

#include <bisemutum/runtime/component_manager.hpp>

#include <bisemutum/math/transform.hpp>

#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/scene_basic/light.hpp>
#include <bisemutum/scene_basic/skybox.hpp>
#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/mesh_renderer.hpp>

#include <bisemutum/renderer/basic.hpp>
#include <bisemutum/renderer/ddgi_volume.hpp>
#include <bisemutum/renderer/post_process_volume.hpp>

namespace bi {

auto register_components(Ref<rt::ComponentManager> mgr) -> void {
    mgr->register_component<Transform>();

    mgr->register_component<CameraComponent>();
    mgr->register_component<DirectionalLightComponent>();
    mgr->register_component<PointLightComponent>();
    mgr->register_component<RectLightComponent>();
    mgr->register_component<SkyboxComponent>();
    mgr->register_component<StaticMeshComponent>();
    mgr->register_component<MeshRendererComponent>();

    mgr->register_component<BasicRendererOverrideVolume>();
    mgr->register_component<DdgiVolumeComponent>();
    mgr->register_component<PostProcessVolumeComponent>();
}

}

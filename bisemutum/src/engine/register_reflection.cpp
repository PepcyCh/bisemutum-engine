#include "register.hpp"

#include <bisemutum/utils/drefl.hpp>

#include <bisemutum/math/transform.hpp>

#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/scene_basic/light.hpp>
#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/mesh_renderer.hpp>

namespace bi {

auto register_reflections(Ref<drefl::ReflectionManager> mgr) -> void {
    mgr->register_type<Transform>();

    mgr->register_type<CameraComponent>();
    mgr->register_type<LightComponent>();
    mgr->register_type<StaticMeshComponent>();
    mgr->register_type<MeshRendererComponent>();
}

}

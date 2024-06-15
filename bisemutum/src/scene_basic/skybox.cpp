#include <bisemutum/scene_basic/skybox.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/window/window_manager.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>

namespace bi {

struct SkyboxSystem::Impl final {
    auto init_on(Ref<rt::Scene> scene) -> void {
        this->scene = scene;
        scene->ecs_registry().on_construct<SkyboxComponent>().connect<&Impl::on_construct>(this);
        scene->ecs_registry().on_destroy<SkyboxComponent>().connect<&Impl::on_destroy>(this);

    }

    auto current_skybox() -> SkyboxInfo& {
        update_current_skybox();
        return curr_skybox_info;
    }

    auto on_construct(entt::registry& ecs_registry, entt::entity entity) -> void {
        auto object = scene->object_of(entity);
        if (!curr_skybox) {
            curr_skybox = object;
        }
        skybox_objects.insert(object);
    }
    auto on_destroy(entt::registry& ecs_registry, entt::entity entity) -> void {
        auto object = scene->object_of(entity);
        skybox_objects.erase(object);
        if (curr_skybox == object) {
            if (skybox_objects.empty()) {
                curr_skybox.reset();
            } else {
                curr_skybox = *skybox_objects.begin();
            }
        }
    }

    auto update_current_skybox() -> void {
        // Update at most once per frame.
        auto frame = g_engine->window_manager()->frame_count();
        if (last_update_frame == frame) { return; }
        last_update_frame = frame;

        curr_skybox_info.tex = g_engine->graphics_manager()->default_texture(gfx::DefaultTexture::black_1x1_cube);
        curr_skybox_info.color = float3{0.0f};
        curr_skybox_info.diffuse_strength = 0.0f;
        curr_skybox_info.specular_strength = 0.0f;
        if (curr_skybox) {
            auto component = curr_skybox->get_component<SkyboxComponent>();
            if (!component->texture.empty()) {
                component->texture.load();
                if (component->texture.asset()->texture.has_value()) {
                    auto& desc = component->texture.asset()->texture.desc();
                    if (
                        desc.dim == rhi::TextureDimension::d2
                        && desc.extent.width == desc.extent.height
                        && desc.extent.depth_or_layers >= 6
                    ) {
                        curr_skybox_info.tex = &component->texture.asset()->texture;
                        curr_skybox_info.color = component->color * component->strength;
                        curr_skybox_info.diffuse_strength = component->diffuse_strength;
                        curr_skybox_info.specular_strength = component->specular_strength;
                        curr_skybox_info.transform = curr_skybox->world_transform().rotation;
                    }
                }
            }
        }
    }

    Ptr<rt::Scene> scene;

    Ptr<rt::SceneObject> curr_skybox;
    std::unordered_set<Ref<rt::SceneObject>> skybox_objects;

    uint64_t last_update_frame = 0;
    SkyboxInfo curr_skybox_info;
};

SkyboxSystem::SkyboxSystem() = default;

auto SkyboxSystem::init_on(Ref<rt::Scene> scene) -> void {
    impl()->init_on(scene);
}

auto SkyboxSystem::update() -> void {}

auto SkyboxSystem::current_skybox() -> SkyboxInfo& {
    return impl()->current_skybox();
}

}

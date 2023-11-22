#include <bisemutum/runtime/menu_action.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/editor/file_dialog.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/runtime/asset.hpp>
#include <bisemutum/runtime/prefab.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>

namespace bi::editor {

namespace {

auto menu_action_add_prefab_to_scene(MenuActionContext const& ctx) -> void {
    ctx.file_dialog->choose_file(
        "Add Prefab", "Choose Prefab", ".prefab.toml",
        [](std::string choosed_path) {
            if (!std::filesystem::exists(choosed_path)) { return; }
            rt::TAssetPtr<rt::Prefab> prefab{choosed_path};
            prefab.load();
            prefab.asset()->instantiate();
        }
    );
}

} // namespace

auto register_menu_actions_runtime(Ref<editor::MenuManager> mgr) -> void {
    mgr->register_action("Scene/Add/Prefab", {}, menu_action_add_prefab_to_scene);
}

}

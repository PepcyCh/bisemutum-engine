#include <bisemutum/editor/menu_manager.hpp>

#include <imgui.h>
#include <bisemutum/prelude/option.hpp>
#include <bisemutum/containers/hash.hpp>

namespace bi::editor {

namespace {

struct MenuItem final {
    StringHashMap<MenuItem> sub_items;
    BitFlags<MenuItemFlag> flags;
    Option<MenuAction> action;
    bool state = false;
};

}

struct MenuManager::Impl final {
    auto register_action(std::string_view path, BitFlags<MenuItemFlag> flags, MenuAction&& action) -> bool {
        auto curr_items = &items;
        size_t last_p = 0;
        for (auto p = path.find("/"); p != std::string::npos; p = path.find("/", p + 1)) {
            auto part = path.substr(last_p, p - last_p);
            auto it = curr_items->find(part);
            if (it == curr_items->end()) {
                it = curr_items->try_emplace(std::string{part}).first;
            }
            curr_items = &it->second.sub_items;
            last_p = p + 1;
        }
        auto part = path.substr(last_p);
        auto it = curr_items->find(part);
        if (it == curr_items->end()) {
            it = curr_items->try_emplace(std::string{part}).first;
            it->second.action = std::move(action);
            it->second.flags = flags;
            return true;
        } else {
            return false;
        }
    }

    auto display(MenuActionContext& ctx) -> void {
        std::function<auto(StringHashMap<MenuItem>&) -> void> dfs = [&dfs, &ctx](StringHashMap<MenuItem>& items) {
            for (auto& [name, item] : items) {
                if (item.action.has_value()) {
                    if (ImGui::MenuItem(
                        name.c_str(), nullptr,
                        item.flags.contains_any(MenuItemFlag::checkable) ? &item.state : nullptr
                    )) {
                        ctx.state = item.state;
                        (item.action.value())(ctx);
                    }
                } else if (ImGui::BeginMenu(name.c_str())) {
                    dfs(item.sub_items);
                    ImGui::EndMenu();
                }
            }
        };
        dfs(items);
    }

    StringHashMap<MenuItem> items;
};

MenuManager::MenuManager() = default;

auto MenuManager::register_action(std::string_view path, BitFlags<MenuItemFlag> flags, MenuAction action) -> bool {
    return impl()->register_action(path, flags, std::move(action));
}

auto MenuManager::display(MenuActionContext& ctx) -> void {
    impl()->display(ctx);
}

}

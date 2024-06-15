#include <bisemutum/editor/file_dialog.hpp>

#include <ImGuiFileDialog.h>
#include <bisemutum/containers/hash.hpp>

namespace bi::editor {

struct FileDialog::Impl final {
    auto choose_file(char const* key, char const* title, char const* filter, ChooseFileCallback&& callback) -> void {
        std::string key_str{key};
        if (auto it = instances.find(key_str); it == instances.end()) {
            it = instances.try_emplace(std::move(key_str)).first;
            it->second.fd.OpenDialog(key, title, filter, ".");
            it->second.callback = std::move(callback);
        }
    }

    auto update() -> void {
        std::vector<decltype(instances)::iterator> to_be_deleted;
        for (auto it = instances.begin(); it != instances.end(); it++) {
            if (it->second.fd.Display(it->first)) {
                if (it->second.fd.IsOk()) {
                    it->second.callback(it->second.fd.GetFilePathName());
                    to_be_deleted.push_back(it);
                }
                it->second.fd.Close();
            }
        }
        for (auto it : to_be_deleted) {
            instances.erase(it);
        }
    }

    struct Instance final {
        ImGuiFileDialog fd;
        ChooseFileCallback callback;
    };
    StringHashMap<Instance> instances;
};

FileDialog::FileDialog() = default;

FileDialog::FileDialog(FileDialog&& rhs) = default;
auto FileDialog::operator=(FileDialog&& rhs) -> FileDialog& = default;

auto FileDialog::update() -> void {
    impl()->update();
}

auto FileDialog::choose_file(
    char const* key, char const* title, char const* filter, ChooseFileCallback callback
) -> void {
    impl()->choose_file(key, title, filter, std::move(callback));
}

}

#include <bisemutum/runtime/vfs.hpp>

#include <list>
#include <variant>
#include <fstream>

#include <bisemutum/prelude/ref.hpp>
#include <bisemutum/containers/hash.hpp>

namespace bi::rt {

namespace {

auto normalize_path(std::string_view path) -> std::string {
    std::vector<std::string_view> parts{};
    size_t last_p = 0;
    for (auto p = path.find_first_of("/\\"); p != std::string::npos; p = path.find_first_of("/\\", p + 1)) {
        parts.push_back(path.substr(last_p, p - last_p));
        last_p = p + 1;
    }
    parts.push_back(path.substr(last_p));

    std::vector<size_t> selected_parts{};
    selected_parts.reserve(parts.size());
    for (size_t i = 0; i < parts.size(); i++) {
        const auto &s = parts[i];
        if (s == "..") {
            if (!selected_parts.empty() && parts[selected_parts.back()] != "..") {
                selected_parts.pop_back();
            } else {
                selected_parts.push_back(i);
            }
        } else if (!s.empty() && s != ".") {
            selected_parts.push_back(i);
        }
    }

    std::string normalized_path{};
    for (auto i : selected_parts) {
        normalized_path += std::string{parts[i]} + '/';
    }
    if (!normalized_path.empty()) {
        normalized_path.pop_back();
    }
    return normalized_path;
}

struct VirtualDirectory final {
    auto create_dir(std::string_view path) -> Ptr<VirtualDirectory> {
        auto normalized_path = normalize_path(path);
        if (normalized_path.empty()) {
            return unsafe_make_ref(this);
        }

        auto curr_dir = unsafe_make_ref(this);
        auto p = normalized_path.find('/');
        size_t last_p = 0;
        while (true) {
            bool is_end = p == std::string::npos;
            p = is_end ? normalized_path.size() : p;
            auto name = std::string_view{normalized_path.data() + last_p, p - last_p};
            if (name == "..") {
                return nullptr;
            }
            if (auto it = curr_dir->name_map_.find(name); it != curr_dir->name_map_.end()) {
                curr_dir = it->second;
                if (is_end) {
                    return curr_dir;
                }
            } else {
                auto &dir = curr_dir->subdirs_.emplace_back();
                it = curr_dir->name_map_.insert({std::string{name}, unsafe_make_ref(&dir)}).first;
                curr_dir = it->second;
                if (is_end) {
                    return curr_dir;
                }
            }
            last_p = p + 1;
            p = normalized_path.find('/', last_p);
        }
    }

    auto get_dir(std::string_view path) -> Ptr<VirtualDirectory> {
        return get_dir_impl(this, path);
    }
    auto get_dir(std::string_view path) const -> CPtr<VirtualDirectory> {
        return get_dir_impl(this, path);
    }

    auto collect_sub_fs(std::string_view path) const {
        std::vector<std::pair<std::string, Dyn<ISubFileSystem>::CRef>> sub_fs_pars{};

        auto normalized_path = normalize_path(path);
        if (normalized_path.empty()) {
            return sub_fs_pars;
        }

        auto curr_dir = unsafe_make_cref(this);
        auto p = normalized_path.find('/');
        size_t last_p = 0;
        while (true) {
            if (p == std::string::npos) {
                break;
            }
            auto name = std::string_view{normalized_path.data() + last_p, p - last_p};
            if (name == "..") {
                break;
            }
            if (curr_dir->has_sub_fs()) {
                sub_fs_pars.emplace_back(normalized_path.substr(last_p), *&curr_dir->sub_fs_.value());
            }
            if (auto it = curr_dir->name_map_.find(name); it != curr_dir->name_map_.end()) {
                curr_dir = it->second;
            } else {
                break;
            }
            last_p = p + 1;
            p = normalized_path.find('/', last_p);
        }
        if (curr_dir->has_sub_fs()) {
            sub_fs_pars.emplace_back(normalized_path.substr(last_p), *&curr_dir->sub_fs_.value());
        }
        return sub_fs_pars;
    }
    auto collect_writable_sub_fs(std::string_view path) {
        std::vector<std::pair<std::string, Dyn<ISubFileSystem>::Ref>> sub_fs_pars{};

        auto normalized_path = normalize_path(path);
        if (normalized_path.empty()) {
            return sub_fs_pars;
        }

        auto curr_dir = unsafe_make_ref(this);
        auto p = normalized_path.find('/');
        size_t last_p = 0;
        while (true) {
            if (p == std::string::npos) {
                break;
            }
            auto name = std::string_view{normalized_path.data() + last_p, p - last_p};
            if (name == "..") {
                break;
            }
            if (curr_dir->has_sub_fs() && curr_dir->get_sub_fs()->is_writable()) {
                sub_fs_pars.emplace_back(normalized_path.substr(last_p), *&curr_dir->sub_fs_.value());
            }
            if (auto it = curr_dir->name_map_.find(name); it != curr_dir->name_map_.end()) {
                curr_dir = it->second;
            } else {
                break;
            }
            last_p = p + 1;
            p = normalized_path.find('/', last_p);
        }
        if (curr_dir->has_sub_fs()) {
            sub_fs_pars.emplace_back(normalized_path.substr(last_p), *&curr_dir->sub_fs_.value());
        }
        return sub_fs_pars;
    }

    auto has_sub_fs() const -> bool { return sub_fs_.has_value(); }
    auto get_sub_fs() -> Dyn<ISubFileSystem>::Ptr { return &sub_fs_.value(); }
    auto get_sub_fs() const -> Dyn<ISubFileSystem>::CPtr { return &sub_fs_.value(); }
    auto set_sub_fs(Dyn<ISubFileSystem>::Box&& sub_fs) -> void  { sub_fs_ = std::move(sub_fs); }
    auto reset_sub_fs() { sub_fs_.reset(); }

private:
    template <typename Self>
    static auto get_dir_impl(Self *self, std::string_view path) -> Ptr<Self> {
        auto normalized_path = normalize_path(path);
        if (normalized_path.empty()) {
            return unsafe_make_ref(self);
        }

        auto curr_dir = unsafe_make_ref(self);
        auto p = normalized_path.find('/');
        size_t last_p = 0;
        while (true) {
            bool is_end = p == std::string::npos;
            p = is_end ? normalized_path.size() : p;
            auto name = std::string_view{normalized_path.data() + last_p, p - last_p};
            if (name == "..") {
                return nullptr;
            }
            if (auto it = curr_dir->name_map_.find(name); it != curr_dir->name_map_.end()) {
                curr_dir = it->second;
                if (is_end) {
                    return curr_dir;
                }
            } else {
                return nullptr;
            }
            last_p = p + 1;
            p = normalized_path.find('/', last_p);
        }
    }

    std::list<VirtualDirectory> subdirs_;
    StringHashMap<Ref<VirtualDirectory>> name_map_;
    Option<Dyn<ISubFileSystem>::Box> sub_fs_;
};

} // namespace

struct FileSystem::Impl final {
    auto mount(std::string_view target, Dyn<ISubFileSystem>::Box&& sub_fs) -> bool {
        auto dir = root.create_dir(target);
        if (dir->has_sub_fs()) {
            return false;
        } else {
            dir->set_sub_fs(std::move(sub_fs));
            return true;
        }
    }

    auto umount(std::string_view target) -> bool {
        auto dir = root.get_dir(target);
        if (dir.has_value() && dir->has_sub_fs()) {
            dir->reset_sub_fs();
            return true;
        } else {
            return false;
        }
    }

    auto has_file(std::string_view path) const -> bool {
        auto file = get_file(path);
        return file.has_value();
    }

    auto get_file(std::string_view path) const -> Option<Dyn<IFile>::Box> {
        auto sub_fs_pairs = root.collect_sub_fs(path);
        for (auto it = sub_fs_pairs.rbegin(); it != sub_fs_pairs.rend(); it++) {
            if (auto file = it->second.get_file(it->first); file.has_value()) {
                return file;
            }
        }
        return {};
    }

    auto create_file(std::string_view path) -> Option<Dyn<IFile>::Box> {
        auto sub_fs_pairs = root.collect_writable_sub_fs(path);
        for (auto it = sub_fs_pairs.rbegin(); it != sub_fs_pairs.rend(); it++) {
            if (auto file = it->second.create_file(it->first); file.has_value()) {
                return file;
            }
        }
        return {};
    }

    auto remove_file(std::string_view path) -> bool {
        auto sub_fs_pairs = root.collect_writable_sub_fs(path);
        for (auto it = sub_fs_pairs.rbegin(); it != sub_fs_pairs.rend(); it++) {
            if (it->second.remove_file(it->first)) {
                return true;
            }
        }
        return false;
    }

    VirtualDirectory root;
};

FileSystem::FileSystem() = default;
FileSystem::~FileSystem() = default;

auto FileSystem::mount(std::string_view target, Dyn<ISubFileSystem>::Box sub_fs) -> bool {
    return impl()->mount(target, std::move(sub_fs));
}
auto FileSystem::umount(std::string_view target) -> bool {
    return impl()->umount(target);
}

auto FileSystem::has_file(std::string_view path) const -> bool {
    return impl()->has_file(path);
}
auto FileSystem::get_file(std::string_view path) const -> Option<Dyn<IFile>::Box> {
    return impl()->get_file(path);
}
auto FileSystem::create_file(std::string_view path) -> Option<Dyn<IFile>::Box> {
    return impl()->create_file(path);
}
auto FileSystem::remove_file(std::string_view path) -> bool {
    return impl()->remove_file(path);
}


PhysicalFile::PhysicalFile(std::filesystem::path path, bool writable) : path_(std::move(path)), writable_(writable) {}

auto PhysicalFile::read_string_data() -> std::string {
    std::ifstream fin(path_, std::ios::ate);
    if (!fin) { return {}; }
    auto size = fin.tellg();
    std::string data(size, '\0');
    fin.seekg(0);
    fin.read(data.data(), size);
    while (!data.empty() && data.back() == '\0') { data.pop_back(); }
    return data;
}
auto PhysicalFile::read_binary_data() -> std::vector<std::byte> {
    std::ifstream fin(path_, std::ios::ate | std::ios::binary);
    if (!fin) { return {}; }
    auto size = fin.tellg();
    std::vector<std::byte> data(size, {});
    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

auto PhysicalFile::write_string_data(std::string_view data) -> bool {
    std::ofstream fout(path_);
    if (!fout) { return false; }
    fout.write(data.data(), data.size());
    return true;
}
auto PhysicalFile::write_binary_data(CSpan<std::byte> data) -> bool {
    std::ofstream fout(path_, std::ios::binary);
    if (!fout) { return false; }
    fout.write(reinterpret_cast<char const*>(data.data()), data.size());
    return true;
}


PhysicalSubFileSystem::PhysicalSubFileSystem(std::filesystem::path root_path, bool writable)
    : root_path_(std::move(root_path)), writable_(writable) {}

auto PhysicalSubFileSystem::has_file(std::string_view path) const -> bool {
    auto full_path = root_path_ / path;
    return std::filesystem::is_regular_file(full_path);
}
auto PhysicalSubFileSystem::get_file(std::string_view path) const -> Option<Dyn<IFile>::Box> {
    auto full_path = root_path_ / path;
    if (std::filesystem::is_regular_file(full_path)) {
        return Dyn<IFile>::Box{PhysicalFile(std::move(full_path), writable_)};
    } else {
        return {};
    }
}

auto PhysicalSubFileSystem::create_file(std::string_view path) -> Option<Dyn<IFile>::Box> {
    if (!writable_) { return {}; }

    auto full_path = root_path_ / path;
    if (std::filesystem::is_regular_file(full_path)) {
        return {};
    } else {
        return Dyn<IFile>::Box{PhysicalFile(std::move(full_path), writable_)};
    }
}
auto PhysicalSubFileSystem::remove_file(std::string_view path) -> bool {
    if (!writable_) { return false; }

    auto full_path = root_path_ / path;
    if (std::filesystem::is_regular_file(full_path)) {
        std::filesystem::remove(full_path);
        return true;
    } else {
        return false;
    }
}

}
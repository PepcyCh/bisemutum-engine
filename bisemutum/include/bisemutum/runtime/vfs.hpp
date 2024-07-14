#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "../prelude/span.hpp"
#include "../prelude/option.hpp"
#include "../prelude/poly.hpp"
#include "../prelude/idiom.hpp"

namespace bi::rt {

BI_TRAIT_BEGIN(IFile, move)
    BI_TRAIT_METHOD(is_writable, (const& self) requires (self.is_writable()) -> bool)
    BI_TRAIT_METHOD(filename, (const& self) requires (self.filename()) -> std::string)
    BI_TRAIT_METHOD(extension, (const& self) requires (self.extension()) -> std::string)
    BI_TRAIT_METHOD(read_string_data, (&self) requires (self.read_string_data()) -> std::string)
    BI_TRAIT_METHOD(read_binary_data, (&self) requires (self.read_binary_data()) -> std::vector<std::byte>)
    BI_TRAIT_METHOD(write_string_data, (&self, std::string_view data) requires (self.write_string_data(data)) -> bool)
    BI_TRAIT_METHOD(write_binary_data, (&self, CSpan<std::byte> data) requires (self.write_binary_data(data)) -> bool)
BI_TRAIT_END(IFile)

BI_TRAIT_BEGIN(ISubFileSystem, move)
    BI_TRAIT_METHOD(is_writable, (const& self) requires (self.is_writable()) -> bool)
    BI_TRAIT_METHOD(has_file, (const& self, std::string_view path) requires (self.has_file(path)) -> bool)
    BI_TRAIT_METHOD(get_file,
        (const& self, std::string_view path) requires (self.get_file(path)) -> Option<Dyn<IFile>::Box>
    )
    BI_TRAIT_METHOD(create_file,
        (&self, std::string_view path) requires (self.create_file(path)) -> Option<Dyn<IFile>::Box>
    )
    BI_TRAIT_METHOD(remove_file, (&self, std::string_view path) requires (self.remove_file(path)) -> bool)
    BI_TRAIT_METHOD(get_physical_path, (const& self) requires (self.get_physical_path()) -> std::filesystem::path)
BI_TRAIT_END(ISubFileSystem)

struct FileSystem final : PImpl<FileSystem> {
    struct Impl;

    FileSystem();

    auto mount(std::string_view target, Dyn<ISubFileSystem>::Box sub_fs) -> bool;
    auto umount(std::string_view target) -> bool;

    auto has_file(std::string_view path) const -> bool;
    auto get_file(std::string_view path) const -> Option<Dyn<IFile>::Box>;
    auto create_file(std::string_view path) -> Option<Dyn<IFile>::Box>;
    auto remove_file(std::string_view path) -> bool;

    auto try_convert_physical_path_to_vfs_path(std::filesystem::path const& path) -> std::string;
};


struct PhysicalFile final {
    PhysicalFile(std::filesystem::path path, bool writable);

    auto is_writable() const -> bool { return writable_; }
    auto filename() const -> std::string { return path_.filename().string(); }
    auto extension() const -> std::string { return path_.extension().string(); }

    auto read_string_data() -> std::string;
    auto read_binary_data() -> std::vector<std::byte>;

    auto write_string_data(std::string_view data) -> bool;
    auto write_binary_data(CSpan<std::byte> data) -> bool;

private:
    std::filesystem::path path_;
    bool writable_;
};

struct PhysicalSubFileSystem final {
    PhysicalSubFileSystem(std::filesystem::path root_path, bool writable = true);

    auto is_writable() const -> bool { return writable_; }

    auto has_file(std::string_view path) const -> bool;
    auto get_file(std::string_view path) const -> Option<Dyn<IFile>::Box>;

    auto create_file(std::string_view path) -> Option<Dyn<IFile>::Box>;
    auto remove_file(std::string_view path) -> bool;

    auto get_physical_path() const -> std::filesystem::path { return root_path_; }

private:
    std::filesystem::path root_path_;
    bool writable_;
};


struct MemorySubFileSystem;

struct MemoryFile final : PImpl<MemoryFile>  {
    struct Impl;

    auto is_writable() const -> bool;
    auto filename() const -> std::string;
    auto extension() const -> std::string;

    auto read_string_data() -> std::string;
    auto read_binary_data() -> std::vector<std::byte>;

    auto write_string_data(std::string_view data) -> bool;
    auto write_binary_data(CSpan<std::byte> data) -> bool;

private:
    friend MemorySubFileSystem;
    MemoryFile(std::string filename, bool writable);
};

struct MemorySubFileSystem final : PImpl<MemorySubFileSystem> {
    struct Impl;

    MemorySubFileSystem(bool writable = true);

    auto is_writable() const -> bool;

    auto has_file(std::string_view path) const -> bool;
    auto get_file(std::string_view path) const -> Option<Dyn<IFile>::Box>;

    auto create_file(std::string_view path) -> Option<Dyn<IFile>::Box>;
    auto remove_file(std::string_view path) -> bool;

    auto get_physical_path() const -> std::filesystem::path { return {}; }
};

}

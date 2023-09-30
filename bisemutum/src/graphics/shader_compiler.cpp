#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <bisemutum/graphics/shader_compiler.hpp>

#include <chrono>
#include <locale>
#include <codecvt>

#include <fmt/format.h>
#include <cprep/cprep.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/rhi/device.hpp>
#ifdef _WIN32
#include <wrl.h>
template <typename T>
using CComPtr = Microsoft::WRL::ComPtr<T>;
#include <dxcapi.h>
#else
#endif

namespace bi::gfx {

namespace {

struct ShaderIncluder final : pep::cprep::ShaderIncluder {
    auto require_header(std::string_view header_name, std::string_view file_path, Result &result) -> bool override {
        auto vfs = g_engine->file_system();

        auto rel_path = fmt::format("{}/../{}", file_path, header_name);
        if (auto file = vfs->get_file(rel_path); file) {
            loaded_files_.push_back(file.value().read_string_data());
            result.header_content = loaded_files_.back();
            result.header_path = rel_path;
            return true;
        }

        if (auto file = vfs->get_file(header_name); file) {
            loaded_files_.push_back(file.value().read_string_data());
            result.header_content = loaded_files_.back();
            result.header_path = header_name;
            return true;
        }

        return false;
    }

    auto clear() -> void override {
        loaded_files_.clear();
    }

private:
    std::list<std::string> loaded_files_;
};

auto chars_to_wstring(std::string_view str) -> std::wstring {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter{};
    return converter.from_bytes(str.data(), str.data() + str.size());
}

constexpr std::string_view shader_binaries_directory = "/project/binaries/shaders/";
constexpr std::string_view shader_binary_info_file_path = "/project/binaries/shaders/binary_info.db";
constexpr uint32_t shader_binary_info_file_magic_number = 0x5373d269;

} // namespace

struct ShaderCompiler::Impl final {
    ~Impl() {
        save_shader_binary_info_file();
    }

    auto initialize(Ref<rhi::Device> device) -> void {
        this->device = device;
        switch (device->get_backend()) {
            case rhi::Backend::vulkan:
                compiled_shader_suffix = ".spv";
                break;
            case rhi::Backend::d3d12:
                compiled_shader_suffix = ".dxil";
                break;
        }

        read_shader_binary_info_file();

        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));

        dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);
    }

    auto compile_shader(
        std::string_view source_path,
        std::string_view entry,
        rhi::ShaderStage shader_stage,
        ShaderCompilationEnvironment const& environment
    ) -> Expected<Ref<rhi::ShaderModule>, std::string> {
        auto shader_source = process_shader_include_and_macro(source_path, environment);
        DxcBuffer dxc_source{
            .Ptr = shader_source.c_str(),
            .Size = shader_source.size(),
            .Encoding = DXC_CP_UTF8,
        };
        auto shader_key = fmt::format("{} {} {}", source_path, entry, environment.get_config_identifier());
        auto& shader_binary_info = get_shader_binary_info(std::move(shader_key));
        shader_binary_info.source_path = source_path;
        shader_binary_info.entry = entry;
        auto shader_hash = std::hash<std::string>{}(shader_source);
        auto old_shader_binary_path = get_compiled_shader_path(shader_binary_info);
        auto need_to_compile = shader_binary_info.shader_hash != shader_hash;
        shader_binary_info.shader_hash = shader_hash;
        auto shader_binary_path = get_compiled_shader_path(shader_binary_info);
        auto compiled_shader_file = g_engine->file_system()->get_file(shader_binary_path);
        need_to_compile |= !compiled_shader_file.has_value();
        if (need_to_compile && g_engine->file_system()->has_file(old_shader_binary_path)) {
            g_engine->file_system()->remove_file(old_shader_binary_path);
        }
        if (!need_to_compile) {
            if (auto it = cached_shader_module.find(shader_binary_info.shader_key); it != cached_shader_module.end()) {
                return it->second.ref();
            }
            auto compiled_shader = compiled_shader_file.value().read_binary_data();
            rhi::ShaderModuleDesc sm_desc{
                .binary_data = compiled_shader,
            };
            auto it = cached_shader_module.insert(
                {shader_binary_info.shader_key, device->create_shader_module(sm_desc)}
            ).first;
            return it->second.ref();
        }

        std::list<std::wstring> owned_args{};
        std::vector<LPCWSTR> args{
            L"-HV", L"2021",
        };
        owned_args.push_back(chars_to_wstring(source_path));
        args.push_back(owned_args.back().c_str());
        owned_args.push_back(chars_to_wstring(entry));
        args.push_back(L"-E");
        args.push_back(owned_args.back().c_str());

        args.push_back(L"-T");
        switch (shader_stage) {
            case rhi::ShaderStage::vertex:
                args.push_back(L"vs_6_6");
                break;
            case rhi::ShaderStage::tessellation_control:
                args.push_back(L"ds_6_6");
                break;
            case rhi::ShaderStage::tessellation_evaluation:
                args.push_back(L"hs_6_6");
                break;
            case rhi::ShaderStage::geometry:
                args.push_back(L"gs_6_6");
                break;
            case rhi::ShaderStage::fragment:
                args.push_back(L"ps_6_6");
                break;
            case rhi::ShaderStage::compute:
                args.push_back(L"cs_6_6");
                break;
            case rhi::ShaderStage::task:
                args.push_back(L"as_6_6");
                break;
            case rhi::ShaderStage::mesh:
                args.push_back(L"ms_6_6");
                break;
            default:
                args.push_back(L"lib_6_6");
                break;
        }

        if (device->get_backend() != rhi::Backend::d3d12) {
            args.push_back(L"-spirv");
        }

        CComPtr<IDxcResult> result;
        dxc_compiler->Compile(&dxc_source, args.data(), args.size(), dxc_include_handler.Get(), IID_PPV_ARGS(&result));

        HRESULT result_status;
        result->GetStatus(&result_status);
        CComPtr<IDxcBlobUtf8> errors = nullptr;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0 && FAILED(result_status)) {
            return std::string{errors->GetStringPointer(), errors->GetStringLength()};
        }

        CComPtr<IDxcBlob> compiled_shader;
        result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiled_shader), nullptr);
        if (compiled_shader) {
            rhi::ShaderModuleDesc sm_desc{
                .binary_data = {
                    reinterpret_cast<std::byte*>(compiled_shader->GetBufferPointer()), compiled_shader->GetBufferSize()
                },
            };
            auto it = cached_shader_module.insert(
                {shader_binary_info.shader_key, device->create_shader_module(sm_desc)}
            ).first;
            return it->second.ref();
        }

        return std::string{"Unknown compilation error."};
    }

    auto process_shader_include_and_macro(
        std::string_view source_path,
        ShaderCompilationEnvironment const& environment
    ) -> std::string {
        auto vfs = g_engine->file_system();
        auto file = vfs->get_file(source_path);
        auto shader_content = file.value().read_string_data();

        std::list<std::string> options_owned_str{};
        std::vector<std::string_view> options(environment.defines.size() * 2);
        options.reserve(environment.defines.size() * 2);
        for (size_t index = 0; auto& [key, value] : environment.defines) {
            options[index] = "-D";
            ++index;
            if (value.empty()) {
                options[index] = key;
            } else {
                auto& opt = options_owned_str.emplace_back();
                opt = fmt::format("{}={}", key, value);
                options[index] = opt;
            }
            ++index;
        }

        ShaderIncluder shader_includer{};
        auto result = shader_preprocessor.do_preprocess(
            source_path, shader_content, shader_includer, options.data(), options.size()
        );
        if (!result.error.empty()) {
            return "";
        }

        for (const auto &[key, content] : environment.replace_args) {
            for (
                size_t pos = 0;
                (pos = result.parsed_result.find(key.c_str(), pos, key.size())) != std::string::npos;
                pos += content.size()
            ) {
                result.parsed_result.replace(pos, key.size(), content.data(), content.size());
            }
        }
        return result.parsed_result;
    }

    struct ShaderBinaryInfo final {
        std::string shader_key;
        std::string source_path;
        std::string entry;
        uint64_t shader_hash;
        uint64_t last_used_timestamp;
    };

    auto read_shader_binary_info_file() -> void {
        auto file = g_engine->file_system()->get_file(shader_binary_info_file_path);
        if (file) {
            auto data = file.value().read_binary_data();
            auto p_data = data.data();
            auto p_data_end = p_data + data.size();

            auto magic_number = *reinterpret_cast<uint32_t*>(p_data);
            p_data += sizeof(uint32_t);
            if (magic_number != shader_binary_info_file_magic_number) { return; }

            auto num_info = *reinterpret_cast<uint32_t*>(p_data);
            p_data += sizeof(uint32_t);

            shader_binary_infos.resize(num_info);
            shader_binary_info_path_map.reserve(num_info);
            for (uint32_t i = 0; i < num_info; i++) {
                auto path_length = *reinterpret_cast<uint32_t*>(p_data);
                p_data += sizeof(uint32_t);
                shader_binary_infos[i].source_path = std::string{reinterpret_cast<char*>(p_data), path_length};
                p_data += path_length;
                auto entry_length = *reinterpret_cast<uint32_t*>(p_data);
                p_data += sizeof(uint32_t);
                shader_binary_infos[i].entry = std::string{reinterpret_cast<char*>(p_data), entry_length};
                p_data += entry_length;
                shader_binary_infos[i].shader_hash = *reinterpret_cast<uint64_t*>(p_data);
                p_data += sizeof(uint64_t);
                shader_binary_infos[i].last_used_timestamp = *reinterpret_cast<uint64_t*>(p_data);
                p_data += sizeof(uint64_t);
                shader_binary_info_path_map.insert({shader_binary_infos[i].shader_key, i});
            }
        }
    }

    auto save_shader_binary_info_file() -> void {
        std::sort(
            shader_binary_infos.begin(), shader_binary_infos.end(),
            [](ShaderBinaryInfo const& a, ShaderBinaryInfo const& b) {
                return a.last_used_timestamp > b.last_used_timestamp;
            }
        );
        using namespace std::chrono_literals;
        uint64_t remove_time_threshold = (std::chrono::system_clock::now() - (30 * 24h)).time_since_epoch().count();
        while (!shader_binary_infos.empty()) {
            auto const& info = shader_binary_infos.back();
            if (info.last_used_timestamp < remove_time_threshold) {
                g_engine->file_system()->remove_file(get_compiled_shader_path(info));
                shader_binary_info_path_map.erase(info.shader_key);
                shader_binary_infos.pop_back();
            } else {
                break;
            }
        }
        // TODO - write file
    }

    auto get_compiled_shader_path(ShaderBinaryInfo const& info) -> std::string {
        return fmt::format("{}{}{}", shader_binaries_directory, info.source_path, compiled_shader_suffix);
    }

    auto get_shader_binary_info(std::string&& shader_key) -> ShaderBinaryInfo& {
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        if (auto it = shader_binary_info_path_map.find(shader_key); it != shader_binary_info_path_map.end()) {
            auto& info = shader_binary_infos[it->second];
            info.last_used_timestamp = timestamp;
            return info;
        }
        auto& info = shader_binary_infos.emplace_back();
        info.shader_key = std::move(shader_key);
        info.shader_hash = 0;
        info.last_used_timestamp = timestamp;
        shader_binary_info_path_map.insert({info.shader_key, shader_binary_infos.size()});
        return info;
    }

    Ptr<rhi::Device> device;
    std::string_view compiled_shader_suffix;

    CComPtr<IDxcUtils> dxc_utils;
    CComPtr<IDxcCompiler3> dxc_compiler;
    CComPtr<IDxcIncludeHandler> dxc_include_handler;

    pep::cprep::Preprocessor shader_preprocessor;

    std::vector<ShaderBinaryInfo> shader_binary_infos;
    std::unordered_map<std::string_view, size_t> shader_binary_info_path_map;
    std::unordered_map<std::string_view, Box<rhi::ShaderModule>> cached_shader_module;
};

ShaderCompiler::ShaderCompiler() = default;

ShaderCompiler::~ShaderCompiler() = default;

auto ShaderCompiler::initialize(Ref<rhi::Device> device) -> void {
    impl()->initialize(device);
}

auto ShaderCompiler::compile_shader(
    std::string_view source_path,
    std::string_view entry,
    rhi::ShaderStage shader_stage,
    ShaderCompilationEnvironment const& environment
) -> Expected<Ref<rhi::ShaderModule>, std::string> {
    return impl()->compile_shader(source_path, entry, shader_stage, environment);
}

}

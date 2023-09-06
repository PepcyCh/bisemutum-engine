#include <filesystem>
#include <fstream>
#include <iostream>

#include <cprep/cprep.hpp>

namespace fs = std::filesystem;

std::string read_all_from_file(const fs::path &path) {
    std::ifstream fin(path);
    fin.seekg(0, std::ios::end);
    auto length = fin.tellg();
    std::string content(length, '\0');
    fin.seekg(0, std::ios::beg);
    fin.read(content.data(), length);
    while (!content.empty() && content.back() == '\0') { content.pop_back(); }
    return content;
}

void write_all_to_file(const std::filesystem::path &path, std::string_view content) {
    std::ofstream fout(path);
    if (!fout) {
        auto err_str = "failed to open file '" + path.string() + "' when writing";
        throw std::runtime_error{err_str};
    }
    fout.write(content.data(), content.size());
}

class FsShaderIncluder final : public pep::cprep::ShaderIncluder {
public:
    FsShaderIncluder(std::vector<fs::path> &&include_dirs) : include_dirs_(std::move(include_dirs)) {}

    bool require_header(std::string_view header_name, std::string_view file_path, Result &result) override {
        auto rel_path = fs::path{file_path}.parent_path() / header_name;
        if (fs::exists(rel_path)) {
            result.header_path = rel_path.string();
            result.header_content = read_file(rel_path);
            return true;
        }

        for (const auto &dir : include_dirs_) {
            auto path = dir / header_name;
            if (fs::exists(path)) {
                result.header_path = path.string();
                result.header_content = read_file(path);
                return true;
            }
        }

        return false;
    }

    void clear() override {
        sources_.clear();
    }

private:
    std::string_view read_file(const fs::path &path) {
        sources_.push_back(read_all_from_file(path));
        return sources_.back();
    }

    std::vector<fs::path> include_dirs_;
    std::vector<std::string> sources_;
};

int main(int argc, char **argv) {
    const char *help_str = R"(pep-cprep-bin [options]... <shader-path>

[options]
  -h                   print this help info

  -o <path>            set output file path
                       an output file path must be specified

  -I<path>
  -I <path>            add include directory

  -D<name[=value]>
  -D <name[=value]>    add predefined macro

  -U<name>
  -U <name>            remove defined macro
)";

    fs::path compiled_file{};
    fs::path output_file{};
    std::vector<fs::path> include_dirs;
    std::vector<std::string_view> passed_options;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            std::cout << help_str << std::endl;
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            ++i;
            if (i == argc) {
                std::cerr << "invalid -o option" << std::endl;
                std::cout << help_str << std::endl;
                return -1;
            }
            if (!output_file.empty()) {
                std::cerr << "multiple output files is not allowed" << std::endl;
                std::cout << help_str << std::endl;
                return -1;
            }
            output_file = argv[i];
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            if (argv[i][2] != '\0') {
                include_dirs.push_back(fs::path{argv[i] + 2});
            } else {
                ++i;
                if (i == argc) {
                    std::cerr << "invalid -I option" << std::endl;
                    std::cout << help_str << std::endl;
                    return -1;
                }
                include_dirs.push_back(fs::path{argv[i]});
            }
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            if (argv[i][2] != '\0') {
                passed_options.push_back(argv[i]);
            } else {
                ++i;
                if (i == argc) {
                    std::cerr << "invalid -D option" << std::endl;
                    std::cout << help_str << std::endl;
                    return -1;
                }
                passed_options.push_back("-D");
                passed_options.push_back(argv[i]);
            }
        } else if (strncmp(argv[i], "-U", 2) == 0) {
            if (argv[i][2] != '\0') {
                passed_options.push_back(argv[i]);
            } else {
                ++i;
                if (i == argc) {
                    std::cerr << "invalid -U option" << std::endl;
                    std::cout << help_str << std::endl;
                    return -1;
                }
                passed_options.push_back("-U");
                passed_options.push_back(argv[i]);
            }
        } else {
            if (!compiled_file.empty()) {
                std::cerr << "multiple compiled files is not allowed" << std::endl;
                std::cout << help_str << std::endl;
                return -1;
            }
            compiled_file = argv[i];
            if (!fs::exists(compiled_file)) {
                std::cerr << "compiled file '" << compiled_file << "' not found" << std::endl;
                std::cout << help_str << std::endl;
                return -1;
            }
        }
    }
    if (compiled_file.empty()) {
        std::cerr << "compiled file is not specified" << std::endl;
        std::cout << help_str << std::endl;
        return -1;
    }
    if (output_file.empty()) {
        std::cerr << "output file is not specified" << std::endl;
        std::cout << help_str << std::endl;
        return -1;
    }

    auto source_path = compiled_file.string();
    auto source = read_all_from_file(compiled_file);

    FsShaderIncluder includer{std::move(include_dirs)};

    pep::cprep::Preprocessor preprocessor{};
    auto prep_result = preprocessor.do_preprocess(
        source_path, source, includer, passed_options.data(), passed_options.size()
    );
    
    if (!prep_result.error.empty()) {
        std::cerr << prep_result.error << std::endl;
        return -1;
    }

    write_all_to_file(output_file, prep_result.parsed_result);

    return 0;
}

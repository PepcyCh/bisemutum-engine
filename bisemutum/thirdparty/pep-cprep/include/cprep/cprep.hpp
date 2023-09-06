#pragma once

#include <string>

#include "config.hpp"

PEP_CPREP_NAMESPACE_BEGIN

class ShaderIncluder {
public:
    struct Result final {
        // derived class should own header content
        std::string_view header_content;
        std::string header_path;
    };

    virtual ~ShaderIncluder() = default;

    virtual bool require_header(std::string_view header_name, std::string_view file_path, Result &result) = 0;

    // derived class can release owned header contents in 'clear()'
    virtual void clear() {}
};

class EmptyInclude final : public pep::cprep::ShaderIncluder {
public:
    bool require_header(std::string_view header_name, std::string_view file_path, Result &result) override {
        return false;
    }
};


class Preprocessor final {
public:
    Preprocessor();
    ~Preprocessor();

    Preprocessor(const Preprocessor &rhs) = delete;
    Preprocessor &operator=(const Preprocessor &rhs) = delete;

    Preprocessor(Preprocessor &&rhs);
    Preprocessor &operator=(Preprocessor &&rhs);

    struct Result final {
        std::string parsed_result;
        std::string error;
        std::string warning;
    };

    Result do_preprocess(
        std::string_view input_path,
        std::string_view input_content,
        ShaderIncluder &includer,
        const std::string_view *options = nullptr,
        size_t num_options = 0
    );

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

PEP_CPREP_NAMESPACE_END

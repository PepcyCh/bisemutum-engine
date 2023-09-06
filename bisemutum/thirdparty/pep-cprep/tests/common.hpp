#pragma once

#include <iostream>

#include <cprep/cprep.hpp>

inline std::string show_space(std::string_view in) {
    std::string out{};
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '\n') {
            out += '@';
        }
        out += in[i];
    }
    return out;
}

inline bool expect_ok(
    pep::cprep::Preprocessor &Preprocessor,
    pep::cprep::ShaderIncluder &includer,
    std::string_view in_src,
    std::string_view expected,
    std::string_view *options,
    size_t num_options
) {
    auto result = Preprocessor.do_preprocess("/test.cpp", in_src, includer, options, num_options);
    auto pass = result.parsed_result == expected && result.error.empty() && result.warning.empty();
    if (!pass) {
        std::cout << "expected ('@' marks the end of line):\n" << show_space(expected)
            << "\nget ('@' marks the end of line):\n" << show_space(result.parsed_result)
            << "\nerror:\n" << result.error
            << "\narning:\n" << result.warning
            << std::endl;
    }
    return pass;
}

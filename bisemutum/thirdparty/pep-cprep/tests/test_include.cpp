#include "common.hpp"

class TestIncluder final : public pep::cprep::ShaderIncluder {
public:
    bool require_header(std::string_view header_name, std::string_view file_path, Result &result) override {
        if (header_name == "a.hpp") {
            result.header_path = "/a.hpp";
            result.header_content = "#pragma once\nint func_a();\n";
            return true;
        }
        if (header_name == "b.hpp") {
            result.header_path = "/b.hpp";
            result.header_content = "#ifndef B_HPP_\n#define B_HPP_\nint func_b();\n#endif\n";
            return true;
        }
        return false;
    }
};

bool test1(pep::cprep::Preprocessor &Preprocessor, pep::cprep::ShaderIncluder &includer) {
    auto in_src =
R"(#ifndef FOO
#include "a.hpp"
#endif
#include "a.hpp"
#include <a.hpp>
#define B <b.hpp>
#include B
#include "b.hpp"
int main() {
    return 0;
}
)";
    auto expected =
R"(


#line 1 "/a.hpp"

int func_a();

#line 5 "/test.cpp"


#line 1 "/b.hpp"


int func_b();


#line 8 "/test.cpp"
#line 1 "/b.hpp"





#line 9 "/test.cpp"
int main() {
    return 0;
}
)";
    std::string_view options[]{"-DFOO=1"};
    return expect_ok(Preprocessor, includer, in_src, expected, options, 1);
}

int main() {
    pep::cprep::Preprocessor Preprocessor{};
    TestIncluder includer{};

    auto pass = true;

    pass &= test1(Preprocessor, includer);

    return pass ? 0 : 1;
}

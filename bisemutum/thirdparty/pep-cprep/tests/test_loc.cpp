#include "common.hpp"

class TestIncluder final : public pep::cprep::ShaderIncluder {
public:
    bool require_header(std::string_view header_name, std::string_view file_path, Result &result) override {
        if (header_name == "a.hpp") {
            result.header_path = "/a.hpp";
            result.header_content = "#pragma once\nint a = __LINE__;\nstd::string str = __FILE__;\n";
            return true;
        }
        return false;
    }
};

bool test1(pep::cprep::Preprocessor &Preprocessor, pep::cprep::ShaderIncluder &includer) {
    auto in_src =
R"(int x = __LINE__;
int y = __LINE__;

int z = __LINE__ + \
    __LINE__;
)";
    auto expected =
R"(int x = 1;
int y = 2;

int z = 4 + \
    5;
)";
    return expect_ok(Preprocessor, includer, in_src, expected, nullptr, 0);
}

bool test2(pep::cprep::Preprocessor &Preprocessor, pep::cprep::ShaderIncluder &includer) {
    auto in_src =
R"(int x = __LINE__;
#line 10
x = __LINE__;
int y = __LINE__;
)";
    auto expected =
R"(int x = 1;

x = 10;
int y = 11;
)";
    return expect_ok(Preprocessor, includer, in_src, expected, nullptr, 0);
}

bool test3(pep::cprep::Preprocessor &Preprocessor, pep::cprep::ShaderIncluder &includer) {
    auto in_src =
R"(int x = __LINE__;
#include "a.hpp"
int y = __LINE__;
)";
    auto expected =
R"(int x = 1;
#line 1 "/a.hpp"

int a = 2;
std::string str = "/a.hpp";

#line 3 "/test.cpp"
int y = 3;
)";
    return expect_ok(Preprocessor, includer, in_src, expected, nullptr, 0);
}

int main() {
    pep::cprep::Preprocessor Preprocessor{};
    TestIncluder includer{};

    auto pass = true;

    pass &= test1(Preprocessor, includer);
    pass &= test2(Preprocessor, includer);
    pass &= test3(Preprocessor, includer);

    return pass ? 0 : 1;
}

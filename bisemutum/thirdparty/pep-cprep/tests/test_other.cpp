#include "common.hpp"

bool test1(pep::cprep::Preprocessor &Preprocessor, pep::cprep::ShaderIncluder &includer) {
    auto in_src =
R"(#define FOO abc
int func() { return 1'000'000'000u; }
#if defined(FOO) & 0
void func2() {}
#elif 1 + 4 * 5 / 2 / 2 == ((!!0 ? 2 : 1) << 1 | 4)
struct Foo {
    A<Foo> a;
};
#endif
#define assert(expr) do { \
        if (!(expr)) { \
            abort(); \
        } \
    } while (0)
assert(1);
#define STRINGIFY(x) #x
#define STRINGIFY_MACRO(x) STRINGIFY(x)
STRINGIFY(FOO);
STRINGIFY_MACRO(FOO);
std::string ŝtrīňg = "你好";
)";
    auto expected =
R"(
int func() { return 1'000'000'000u; }



struct Foo {
    A<Foo> a;
};






do {         if (!(1)) {             abort();         }     } while (0);


"FOO";
"abc";
std::string ŝtrīňg = "你好";
)";
    return expect_ok(Preprocessor, includer, in_src, expected, nullptr, 0);
}

int main() {
    pep::cprep::Preprocessor Preprocessor{};
    pep::cprep::EmptyInclude includer{};

    auto pass = true;

    pass &= test1(Preprocessor, includer);

    return pass ? 0 : 1;
}

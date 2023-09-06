# cprep

cprep is a macro and include preprocessor for C-like language written in C++20.

Some features those are not so well supported by certain toolchains (like NDK, emsdk) are not used, like `<format>`. But `requires` is used.

## Basic Usage

```c++
#include <cprep/cprep.hpp>

class Includer final : public pep::cprep::ShaderIncluder {
public:
    bool require_header(std::string_view header_name, std::string_view file_path, Result &result) override {
        // handle `include`
        // ...
    }

    void clear() override {
        // clear read files
    }
};

pep::cprep::Preprocessor preprocessor{};
TestIncluder includer{};
auto in_src_path = "foo.cpp";
auto in_src_count = read_all_from_file(in_src_path);
auto result = preprocessor.do_preprocess(in_src_path, in_src_content, includer);

std::cout << result.parsed_result << std::endl;
std::cout << result.error << std::endl;
std::cout << result.warning << std::endl;
```

## Features

Supported directives
* `define`, `undef`
* `if`, `else`, `elif`, `endif`, `ifdef`, `ifndef`
* `elifdef`, `elifndef`
* `error`, `warning`
* `include`
* `pragma once`
* `line`

Supported features
* basic object-like and function-like macro, stringification `#` and concatenation `##`
* `-D` and `-U` options (both `-Dxxx=xxx` and `-D xxx=xxx` are ok)
* `__FILE__`, `__LINE__`
* variable number of parameters, `__VA_ARGS__`, `__VA_OPT__`
* allow single `'` between numbers in integer or floating-point literal
* customized include handler
* unknown directive, pragma and includes are reserved, and a corresponding warning is added
* UTF-8 input and unicode identifier (but only UTF-8 input is acceptable currently)

## Integration

One can just add the source files to the project or use `add_subdirectory(pep-cprep)` add link to target `pep-cprep` when using CMake.

One can set `CPREP_BUILD_OBJECT_LIB` to ON to make `pep-cprep` an object target, which can be integrated into some static library.

One can define `PEP_CPREP_INLINE_NAMESPACE` in `cprep/config.hpp`, or set variable `CPREP_INLINE_NAMESPACE` before `add_subdirectory()` in CMake, or add `target_compile_definitions()` to target `pep-cperp` to define an inline namespace name. By default, the namespace is `pep::cprep::inline <version>`, if `PEP_CPREP_INLINE_NAMESPACE` is set to `foo` for example, the namespace becomes `pep::cprep::inline foo`. This is useful when cprep is expected to be bundled inside a static library to avoid symbol conflicting.

## Acknowledgement

The source `unicode_ident.cpp` is modified from [Unicode ident](https://github.com/dtolnay/unicode-ident) by dtolnay.

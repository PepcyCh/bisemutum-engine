#pragma once

#ifdef _MSC_VER
#define BI_FUNCTION_SIG __FUNCSIG__
#elif defined(__clang__) || defined(__GNUC__)
#define BI_FUNCTION_SIG __PRETTY_FUNCTION__
#else
#error "Unsupported compiler"
#endif

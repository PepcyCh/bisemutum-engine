// This file is modified from https://github.com/swansontec/map-macro/blob/master/map.h
#pragma once

#define BI_MACRO_IMPL_EVAL0(...) __VA_ARGS__
#define BI_MACRO_IMPL_EVAL1(...) BI_MACRO_IMPL_EVAL0(BI_MACRO_IMPL_EVAL0(BI_MACRO_IMPL_EVAL0(__VA_ARGS__)))
#define BI_MACRO_IMPL_EVAL2(...) BI_MACRO_IMPL_EVAL1(BI_MACRO_IMPL_EVAL1(BI_MACRO_IMPL_EVAL1(__VA_ARGS__)))
#define BI_MACRO_IMPL_EVAL3(...) BI_MACRO_IMPL_EVAL2(BI_MACRO_IMPL_EVAL2(BI_MACRO_IMPL_EVAL2(__VA_ARGS__)))
#define BI_MACRO_IMPL_EVAL4(...) BI_MACRO_IMPL_EVAL3(BI_MACRO_IMPL_EVAL3(BI_MACRO_IMPL_EVAL3(__VA_ARGS__)))
#define BI_MACRO_IMPL_EVAL(...)  BI_MACRO_IMPL_EVAL4(BI_MACRO_IMPL_EVAL4(BI_MACRO_IMPL_EVAL4(__VA_ARGS__)))

#define BI_MACRO_IMPL_MAP_END(...)
#define BI_MACRO_IMPL_MAP_OUT
#define BI_MACRO_IMPL_MAP_COMMA ,

#define BI_MACRO_IMPL_MAP_GET_END2() 0, BI_MACRO_IMPL_MAP_END
#define BI_MACRO_IMPL_MAP_GET_END1(...) BI_MACRO_IMPL_MAP_GET_END2
#define BI_MACRO_IMPL_MAP_GET_END(...) BI_MACRO_IMPL_MAP_GET_END1
#define BI_MACRO_IMPL_MAP_NEXT0(test, next, ...) next BI_MACRO_IMPL_MAP_OUT
#define BI_MACRO_IMPL_MAP_NEXT1(test, next) BI_MACRO_IMPL_MAP_NEXT0(test, next, 0)
#define BI_MACRO_IMPL_MAP_NEXT(test, next)  BI_MACRO_IMPL_MAP_NEXT1(BI_MACRO_IMPL_MAP_GET_END test, next)

#define BI_MACRO_IMPL_MAP0(f, x, peek, ...) f(x) BI_MACRO_IMPL_MAP_NEXT(peek, BI_MACRO_IMPL_MAP1)(f, peek, __VA_ARGS__)
#define BI_MACRO_IMPL_MAP1(f, x, peek, ...) f(x) BI_MACRO_IMPL_MAP_NEXT(peek, BI_MACRO_IMPL_MAP0)(f, peek, __VA_ARGS__)

#define BI_MACRO_IMPL_MAP_LIST_NEXT1(test, next) BI_MACRO_IMPL_MAP_NEXT0(test, BI_MACRO_IMPL_MAP_COMMA next, 0)
#define BI_MACRO_IMPL_MAP_LIST_NEXT(test, next)  BI_MACRO_IMPL_MAP_LIST_NEXT1(BI_MACRO_IMPL_MAP_GET_END test, next)

#define BI_MACRO_IMPL_MAP_LIST0(f, x, peek, ...) f(x) BI_MACRO_IMPL_MAP_LIST_NEXT(peek, BI_MACRO_IMPL_MAP_LIST1)(f, peek, __VA_ARGS__)
#define BI_MACRO_IMPL_MAP_LIST1(f, x, peek, ...) f(x) BI_MACRO_IMPL_MAP_LIST_NEXT(peek, BI_MACRO_IMPL_MAP_LIST0)(f, peek, __VA_ARGS__)

#define BI_MACRO_IMPL_MAP_OP0(op, f, x, peek, ...) op(f, x) BI_MACRO_IMPL_MAP_NEXT(peek, BI_MACRO_IMPL_MAP_OP1)(op, f, peek, __VA_ARGS__)
#define BI_MACRO_IMPL_MAP_OP1(op, f, x, peek, ...) op(f, x) BI_MACRO_IMPL_MAP_NEXT(peek, BI_MACRO_IMPL_MAP_OP0)(op, f, peek, __VA_ARGS__)

#define BI_MACRO_IMPL_MAP_OP_LIST0(op, f, x, peek, ...) op(f, x) BI_MACRO_IMPL_MAP_LIST_NEXT(peek, BI_MACRO_IMPL_MAP_OP_LIST1)(op, f, peek, __VA_ARGS__)
#define BI_MACRO_IMPL_MAP_OP_LIST1(op, f, x, peek, ...) op(f, x) BI_MACRO_IMPL_MAP_LIST_NEXT(peek, BI_MACRO_IMPL_MAP_OP_LIST0)(op, f, peek, __VA_ARGS__)

// Applies the function macro `f` to each of the remaining parameters.
#define BI_MACRO_MAP(f, ...) __VA_OPT__(BI_MACRO_IMPL_EVAL(BI_MACRO_IMPL_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0)))
#define BI_MACRO_MAP_LIST(f, ...) __VA_OPT__(BI_MACRO_IMPL_EVAL(BI_MACRO_IMPL_MAP_LIST1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0)))

// Applies op and f to each of the remaining parameters, where op is a something in the form of op(f, x)
#define BI_MACRO_MAP_OP(op, f, ...) __VA_OPT__(BI_MACRO_IMPL_EVAL(BI_MACRO_IMPL_MAP_OP1(op, f, __VA_ARGS__, ()()(), ()()(), ()()(), 0)))
#define BI_MACRO_MAP_OP_LIST(op, f, ...) __VA_OPT__(BI_MACRO_IMPL_EVAL(BI_MACRO_IMPL_MAP_OP_LIST1(op, f, __VA_ARGS__, ()()(), ()()(), ()()(), 0)))

#define BI_MACRO_MAP_OP_FUNC(f, x) f(x)
#define BI_MACRO_MAP_OP_PREFIX(f, x) f##x
#define BI_MACRO_MAP_OP_POSTFIX(f, x) x##f
#define BI_MACRO_MAP_OP_SCOPE(f, x) f::x

#pragma once

#include <string>
#include <string_view>
#include <cstdint>

#include <cprep/config.hpp>

PEP_CPREP_NAMESPACE_BEGIN

inline constexpr int kCharEof = -1;
inline constexpr int kCharInvaliad = -2;

constexpr bool is_eof(int ch) { return ch == kCharEof; }


// for compiler that doesn't support std::string_view{first, last} ctor
template<class T>
constexpr T *cprep_to_address(T *p) noexcept {
    static_assert(!std::is_function_v<T>);
    return p;
}
template<class T>
constexpr auto cprep_to_address(const T &p) noexcept {
    if constexpr (requires{ std::pointer_traits<T>::to_address(p); }) {
        return std::pointer_traits<T>::to_address(p);
    } else {
        return cprep_to_address(p.operator->());
    }
}
template <typename It, typename End>
constexpr std::string_view make_string_view(It first, End last) {
    return std::string_view{cprep_to_address(first), static_cast<std::string_view::size_type>(last - first)};
}


template <size_t Size>
size_t string_length_of(const char (&s)[Size]) { return Size; }
inline size_t string_length_of(std::string_view s) { return s.size(); }
inline size_t string_length_of(const std::string &s) { return s.size(); }
inline size_t string_length_of(std::string &&s) { return s.size(); }
template <typename... StringLike>
std::string concat_string(StringLike &&... args) {
    std::string s{};
    auto size = ((string_length_of(std::forward<StringLike>(args))) + ...);
    s.reserve(size);
    ((s += std::forward<StringLike>(args)), ...);
    return s;
}
template <size_t Size>
auto to_string_like(const char (&s)[Size]) -> const char (&)[Size] { return s; }
inline std::string_view to_string_like(std::string_view s) { return s; }
inline const std::string &to_string_like(const std::string &s) { return s; }
inline std::string &&to_string_like(std::string &&s) { return std::move(s); }
template <typename T> requires requires (T a) { std::to_string(a); }
std::string to_string_like(const T &v) { return std::to_string(v); }
template <typename... Args>
std::string concat(Args &&... args) {
    return concat_string(to_string_like(std::forward<Args>(args))...);
}


class InputState final {
public:
    InputState(std::string_view str) : p_curr_(str.begin()), p_end_(str.end()) {}

    auto get_p_curr() const { return p_curr_; }
    auto get_p_end() const { return p_end_; }
    auto get_lineno() const { return lineno_; }
    auto get_column() const { return col_; }
    auto at_line_start() const { return line_start_; }

    bool is_end() const { return p_curr_ == p_end_; }

    void increase_lineno() { ++lineno_; col_ = 0; }
    void set_lineno(size_t lineno) { lineno_ = lineno; }
    void set_line_start(bool line_start) { line_start_ = line_start; }

    int look_next_ch() const;
    int look_next_ch(size_t offset) const;
    int get_next_ch();
    void unget_last_ch();

    void skip_next_ch();
    void skip_to_end();
    void skip_chars(size_t count);

    std::string_view get_substr(std::string_view::const_iterator p_start, std::string_view::const_iterator p_end) const;
    std::string_view get_substr(std::string_view::const_iterator p_start, size_t count) const;
    std::string_view get_substr_to_end(std::string_view::const_iterator p_start) const;
    std::string_view get_substr_to_curr(std::string_view::const_iterator p_start) const;

private:
    std::string_view::const_iterator p_curr_{};
    std::string_view::const_iterator p_end_{};
    size_t lineno_ = 1;
    size_t col_ = 0;
    bool line_start_ = true;
};

PEP_CPREP_NAMESPACE_END

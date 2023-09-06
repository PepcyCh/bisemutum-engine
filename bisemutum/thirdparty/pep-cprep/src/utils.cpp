#include "utils.hpp"

PEP_CPREP_NAMESPACE_BEGIN

namespace {

bool byte_starts_with_10(int x) {
    return (x & 0x3f) == (x ^ 0x80);
}

int get_char_from_it(const std::string_view::const_iterator it) {
    int b0 = static_cast<uint8_t>(*it);
    if (b0 == EOF || b0 == '\0') {
        return kCharEof;
    } if ((b0 & 0xf0) == 0xf0) {
        // 4 bytes
        int b1 = static_cast<uint8_t>(*(it + 1));
        int b2 = static_cast<uint8_t>(*(it + 2));
        int b3 = static_cast<uint8_t>(*(it + 3));
        if (!byte_starts_with_10(b1) || !byte_starts_with_10(b2) || !byte_starts_with_10(b3)) { return kCharInvaliad; }
        auto ch = ((b0 & 0x07) << 18) | ((b1 & 0x3f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
        return ch <= 0x10ffff ? ch : kCharInvaliad;
    } else if ((b0 & 0xe0) == 0xe0) {
        // 3 bytes
        int b1 = static_cast<uint8_t>(*(it + 1));
        int b2 = static_cast<uint8_t>(*(it + 2));
        if (!byte_starts_with_10(b1) || !byte_starts_with_10(b2)) { return kCharInvaliad; }
        return ((b0 & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f);
    } else if ((b0 & 0xc0) == 0xc0) {
        // 2 bytes
        int b1 = static_cast<uint8_t>(*(it + 1));
        if (!byte_starts_with_10(b1)) { return kCharInvaliad; }
        return ((b0 & 0x1f) << 6) | (b1 & 0x3f);
    } else if ((b0 & 0x80) == 0) {
        // 1 byte
        return b0;
    }
    return kCharInvaliad;
}

}

int InputState::look_next_ch() const {
    return is_end() ? kCharEof : get_char_from_it(p_curr_);
}

int InputState::look_next_ch(size_t offset) const {
    offset = std::min<size_t>(offset, p_end_ - p_curr_);
    auto p = p_curr_ + offset;
    return p == p_end_ ? kCharEof : get_char_from_it(p);
}

int InputState::get_next_ch() {
    auto ch = look_next_ch();
    skip_next_ch();
    return ch;
}

void InputState::unget_last_ch() {
    while (byte_starts_with_10(static_cast<uint8_t>(*--p_curr_))) {}
}

void InputState::skip_next_ch() {
    if (!is_end()) {
        int b0 = static_cast<uint8_t>(*p_curr_);
        if ((b0 & 0xf0) == 0xf0) {
            p_curr_ += std::min<size_t>(p_end_ - p_curr_, 4);
        } else if ((b0 & 0xe0) == 0xe0) {
            p_curr_ += std::min<size_t>(p_end_ - p_curr_, 3);
        } else if ((b0 & 0xc0) == 0xc0) {
            p_curr_ += std::min<size_t>(p_end_ - p_curr_, 2);
        } else if ((b0 & 0x80) == 0) {
            p_curr_ += 1;
        }
        ++col_;
    }
}

void InputState::skip_chars(size_t count) {
    for (size_t i = 0; i < count && !is_end(); i++) {
        skip_next_ch();
    }
}

void InputState::skip_to_end() {
    p_curr_ = p_end_;
}

std::string_view InputState::get_substr(
    std::string_view::const_iterator p_start, std::string_view::const_iterator p_end
) const {
    return make_string_view(p_start, p_end);
}

std::string_view InputState::get_substr(std::string_view::const_iterator p_start, size_t count) const {
    auto real_count = std::min<size_t>(count, p_end_ - p_start);
    return make_string_view(p_start, p_start + real_count);
}

std::string_view InputState::get_substr_to_end(std::string_view::const_iterator p_start) const {
    return make_string_view(p_start, p_end_);
}

std::string_view InputState::get_substr_to_curr(std::string_view::const_iterator p_start) const {
    return make_string_view(p_start, p_curr_);
}

PEP_CPREP_NAMESPACE_END

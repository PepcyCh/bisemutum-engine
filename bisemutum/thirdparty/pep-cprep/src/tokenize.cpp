#include "tokenize.hpp"

#include <cstring>
#include <string>

#include "unicode_ident.hpp"

PEP_CPREP_NAMESPACE_BEGIN

namespace {

// functions from cctype may abory when input is not in [-1, 255]

bool is_blank(int ch) {
    return ch == ' ' || ch == '\t';
}

bool is_space(int ch) {
    return is_blank(ch) || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

bool is_digit(int ch) {
    return '0' <= ch && ch <= '9';
}

}

Token get_next_token(InputState &input, std::string &output, bool space_cross_line, SpaceKeepType keep) {
    // skip whitespaces and comments
    auto first_ch = input.get_next_ch();
    bool in_ml_comment = false;
    bool in_sl_comment = false;
    while (!is_eof(first_ch)) {
        if (first_ch == '/' && !in_ml_comment && !in_sl_comment) {
            auto second_ch = input.look_next_ch();
            if (second_ch == '*') {
                input.skip_next_ch();
                in_ml_comment = true;
                if ((keep & SpaceKeepType::eSpace) != SpaceKeepType::eNothing) { output += "  "; }
            } else if (second_ch == '/') {
                input.skip_next_ch();
                in_sl_comment = true;
            } else {
                break;
            }
        } else if (first_ch == '*' && in_ml_comment) {
            auto second_ch = input.look_next_ch();
            if (second_ch == '/') {
                input.skip_next_ch();
                in_ml_comment = false;
                if ((keep & SpaceKeepType::eSpace) != SpaceKeepType::eNothing) { output += "  "; }
            }
        } else if (first_ch == '\\') {
            auto second_ch = input.look_next_ch();
            auto third_ch = input.look_next_ch(1);
            if (second_ch == '\n' || (second_ch == '\r' && third_ch == '\n')) {
                input.skip_next_ch();
                if (second_ch == '\r') { input.skip_next_ch(); }
                input.increase_lineno();
                if ((keep & SpaceKeepType::eBackSlash) != SpaceKeepType::eNothing) {
                    output += "\\\n";
                } else if ((keep & SpaceKeepType::eNewLine) != SpaceKeepType::eNothing) {
                    output += '\n';
                }
            }
        } else if (first_ch == '\n') {
            in_sl_comment = false;
            if (space_cross_line) {
                input.increase_lineno();
                if ((keep & SpaceKeepType::eNewLine) != SpaceKeepType::eNothing) { output += '\n'; }
                if (!in_ml_comment && !in_sl_comment) {
                    input.set_line_start(true);
                }
            } else {
                input.unget_last_ch();
                return {TokenType::eEof, {}};
            }
        } else if (!is_space(first_ch) && !in_ml_comment && !in_sl_comment) {
            break;
        } else {
            if ((keep & SpaceKeepType::eSpace) != SpaceKeepType::eNothing) { output += ' '; }
        }
        first_ch = input.get_next_ch();
    }

    if (is_eof(first_ch)) { return {TokenType::eEof, {}}; }
    if (first_ch == kCharInvaliad) { return {TokenType::eUnknown, {}}; }

    // scan token
    input.unget_last_ch();
    const auto p_start = input.get_p_curr();
    input.skip_next_ch();
    auto unknown = [&input, p_start]() {
        while (true) {
            auto ch = input.look_next_ch();
            if (is_space(ch) || is_eof(ch)) { break; }
            input.skip_next_ch();
        }
        return Token{TokenType::eUnknown, input.get_substr_to_curr(p_start)};
    };

    if (first_ch == '"' || first_ch == '\'') {
        auto type = first_ch == '"' ? TokenType::eString : TokenType::eChar;
        bool escape = false;
        while (true) {
            auto ch = input.get_next_ch();
            if (escape) {
                escape = false;
            } else if (ch == '\\') {
                escape = true;
            } else if (ch == first_ch) {
                break;
            } else if (is_eof(ch)) {
                return unknown();
            }
        }
        return {type, input.get_substr(p_start, input.get_p_curr())};
    } else if (first_ch == '#') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '#') {
            input.skip_next_ch();
            return {TokenType::eDoubleSharp, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eSharp, input.get_substr_to_curr(p_start)};
        }
    } else if (is_xid_start(first_ch)) {
        while (true) {
            auto ch = input.look_next_ch();
            if (!is_xid_continue(ch)) { break; }
            input.skip_next_ch();
        }
        return {TokenType::eIdentifier, input.get_substr_to_curr(p_start)};
    } else if (is_digit(first_ch) || first_ch == '.') {
        auto second_ch = input.look_next_ch();
        // number 0
        if (
            first_ch == '0'
            && !is_digit(second_ch) && second_ch != '.'
            && second_ch != 'x' && second_ch != 'X'
            && second_ch != 'b' && second_ch != 'B'
            && second_ch != 'e' && second_ch != 'E'
        ) {
            return {TokenType::eNumber, input.get_substr_to_curr(p_start)};
        }
        // single dot
        if (first_ch == '.' && (is_eof(second_ch) || is_xid_start(second_ch))) {
            return {TokenType::eDot, input.get_substr_to_curr(p_start)};
        }
        // triple dots ...
        if (first_ch == '.' && second_ch == '.') {
            auto third_ch = input.look_next_ch(1);
            if (third_ch == '.') {
                input.skip_chars(2);
                return {TokenType::eTripleDots, input.get_substr_to_curr(p_start)};
            } else {
                return {TokenType::eDot, input.get_substr_to_curr(p_start)};
            }
        }
        // number
        bool has_dot = false;
        bool exp_start = false;
        bool last_exp_start = false;
        bool has_exp = false;
        bool can_be_sep = true;
        auto number_end = input.get_p_end();
        uint32_t base = 10;
        if (first_ch == '0') {
            if (second_ch == '\'') {
                input.skip_next_ch();
                second_ch = input.get_next_ch();
                if (!is_digit(second_ch)) { return unknown(); }
            }
            if (second_ch == 'x' || second_ch == 'X') {
                input.skip_next_ch();
                base = 16;
                can_be_sep = false;
            } else if (second_ch == 'b' || second_ch == 'B') {
                input.skip_next_ch();
                base = 2;
                can_be_sep = false;
            } else if (second_ch == 'e' || second_ch == 'E') {
                input.skip_next_ch();
                last_exp_start = true;
                has_exp = true;
                can_be_sep = false;
            } else if (is_digit(second_ch)) {
                input.skip_next_ch();
                base = 8;
            } else if (second_ch == '.') {
                input.skip_next_ch();
                has_dot = true;
                can_be_sep = false;
            } else {
                number_end = input.get_p_curr();
            }
        } else if (first_ch == '.') {
            has_dot = true;
            can_be_sep = false;
        }
        while (number_end == input.get_p_end() && !input.is_end()) {
            auto ch = input.look_next_ch();
            bool last_is_sep = ch == '\'';
            if (last_is_sep) {
                if (!can_be_sep) { return unknown(); }
                input.skip_next_ch();
                can_be_sep = false;
                continue;
            }
            if (ch == '.') {
                if (has_dot || has_exp || last_is_sep || base == 2) {
                    number_end = input.get_p_curr();
                    break;
                }
                has_dot = true;
                can_be_sep = false;
                if (base == 8) { base = 10; }
            } else if (base != 16 && (ch == 'e' || ch == 'E')) {
                if (has_exp || last_is_sep || base == 2) {
                    number_end = input.get_p_curr();
                    break;
                }
                exp_start = true;
                has_exp = true;
                can_be_sep = false;
                if (base == 8) { base = 10; }
            } else if (base == 16 && (ch == 'p' || ch == 'P')) {
                if (has_exp || last_is_sep) {
                    number_end = input.get_p_curr();
                    break;
                }
                exp_start = true;
                has_exp = true;
                can_be_sep = false;
            } else if (ch == '-' || ch == '+') {
                if (!last_exp_start) {
                    number_end = input.get_p_curr();
                    break;
                }
            } else if ((has_exp || has_dot) && (ch == 'f' || ch == 'F')) {
                number_end = input.get_p_curr();
            } else if (('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F')) {
                if (base != 16 || has_exp) {
                    number_end = input.get_p_curr();
                    break;
                }
                can_be_sep = true;
            } else if (is_digit(ch)) {
                can_be_sep = true;
            } else {
                number_end = input.get_p_curr();
            }
            last_exp_start = exp_start;
            exp_start = false;
            if (number_end == input.get_p_end()) { input.skip_next_ch(); }
        }
        if (base == 8) {
            for (auto it = p_start; it != number_end; it++) {
                if (*it == '8' || *it == '9') { return unknown(); }
            }
        }
        auto remaining = input.get_substr_to_end(number_end);
        static const char *int_valid_suffices[]{
            "ull", "uLL", "ul", "uL", "u",
            "Ull", "ULL", "Ul", "UL", "U",
            "llu", "llU", "ll", "lu", "lU", "l",
            "LLu", "LLU", "LL", "Lu", "LU", "L",
            nullptr,
        };
        static const char *float_valid_suffices[]{
            "f", "l", "F", "L",
            nullptr,
        };
        auto valid_suffices = (has_exp || has_dot) ? float_valid_suffices : int_valid_suffices;
        size_t match_len = 0;
        for (auto p_suffix = valid_suffices; *p_suffix; p_suffix++) {
            if (remaining.starts_with(*p_suffix)) {
                match_len = strlen(*p_suffix);
                break;
            }
        }
        input.skip_chars(match_len);
        number_end += match_len;
        auto number_str = input.get_substr(p_start, number_end);
        return {TokenType::eNumber, number_str};
    } else if (first_ch == '(') {
        return {TokenType::eLeftBracketRound, input.get_substr_to_curr(p_start)};
    } else if (first_ch == '[') {
        return {TokenType::eLeftBracketSquare, input.get_substr_to_curr(p_start)};
    } else if (first_ch == '{') {
        return {TokenType::eLeftBracketCurly, input.get_substr_to_curr(p_start)};
    } else if (first_ch == ')') {
        return {TokenType::eRightBracketRound, input.get_substr_to_curr(p_start)};
    } else if (first_ch == ']') {
        return {TokenType::eRightBracketSquare, input.get_substr_to_curr(p_start)};
    } else if (first_ch == '}') {
        return {TokenType::eRightBracketCurly, input.get_substr_to_curr(p_start)};
    } else if (first_ch == '+') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '+') {
            input.skip_next_ch();
            return {TokenType::eInc, input.get_substr_to_curr(p_start)};
        } else if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eAddEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eAdd, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '-') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '-') {
            input.skip_next_ch();
            return {TokenType::eDec, input.get_substr_to_curr(p_start)};
        } else if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eSubEq, input.get_substr_to_curr(p_start)};
        } else if (second_ch == '>') {
            input.skip_next_ch();
            return {TokenType::eArrow, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eSub, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '*') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eMulEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eMul, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '/') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eDivEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eDiv, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '%') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eModEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eMod, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '&') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '&') {
            input.skip_next_ch();
            return {TokenType::eLAnd, input.get_substr_to_curr(p_start)};
        } else if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eBAndEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eBAnd, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '|') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '|') {
            input.skip_next_ch();
            return {TokenType::eLOr, input.get_substr_to_curr(p_start)};
        } else if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eBOrEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eBOr, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '^') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eBXorEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eBXor, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '~') {
        return {TokenType::eBNot, input.get_substr_to_curr(p_start)};
    } else if (first_ch == '!') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eNotEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eLNot, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '=') {
        auto second_ch = input.look_next_ch();
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eEq, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eAssign, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '<') {
        auto second_ch = input.look_next_ch();
        auto third_ch = input.look_next_ch(1);
        if (second_ch == '=') {
            input.skip_next_ch();
            if (third_ch == '>') {
                input.skip_next_ch();
                return {TokenType::eSpaceship, input.get_substr_to_curr(p_start)};
            } else {
                return {TokenType::eLessEq, input.get_substr_to_curr(p_start)};
            }
        } else if (second_ch == '<') {
            input.skip_next_ch();
            if (third_ch == '=') {
                input.skip_next_ch();
                return {TokenType::eBShlEq, input.get_substr_to_curr(p_start)};
            } else {
                return {TokenType::eBShl, input.get_substr_to_curr(p_start)};
            }
        } else {
            return {TokenType::eLess, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == '>') {
        auto second_ch = input.look_next_ch();
        auto third_ch = input.look_next_ch(1);
        if (second_ch == '=') {
            input.skip_next_ch();
            return {TokenType::eGreaterEq, input.get_substr_to_curr(p_start)};
        } else if (second_ch == '>') {
            input.skip_next_ch();
            if (third_ch == '=') {
                input.skip_next_ch();
                return {TokenType::eBShrEq, input.get_substr_to_curr(p_start)};
            } else {
                return {TokenType::eBShr, input.get_substr_to_curr(p_start)};
            }
        } else {
            return {TokenType::eGreater, input.get_substr_to_curr(p_start)};
        }
    } else if (first_ch == ',') {
        return {TokenType::eComma, input.get_substr_to_curr(p_start)};
    } else if (first_ch == ';') {
        return {TokenType::eSemicolon, input.get_substr_to_curr(p_start)};
    } else if (first_ch == '?') {
        return {TokenType::eQuestion, input.get_substr_to_curr(p_start)};
    } else if (first_ch == ':') {
        auto second_ch = input.look_next_ch();
        if (second_ch == ':') {
            input.skip_next_ch();
            return {TokenType::eScope, input.get_substr_to_curr(p_start)};
        } else {
            return {TokenType::eColon, input.get_substr_to_curr(p_start)};
        }
    }
    input.skip_to_end();
    return unknown();
}

PEP_CPREP_NAMESPACE_END

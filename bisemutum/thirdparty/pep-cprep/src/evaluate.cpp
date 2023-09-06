#include "evaluate.hpp"

#include <vector>
#include <stack>
#include <variant>
#include <cmath>
#include <cctype>

#include "tokenize.hpp"

PEP_CPREP_NAMESPACE_BEGIN

namespace {

[[noreturn]] void unreachable() { std::abort(); }

int char_to_number(char ch) {
    if ('a' <= ch && ch <= 'f') {
        return ch - 'a' + 10;
    } else if ('A' <= ch && ch <= 'F') {
        return ch - 'A' + 10;
    } else {
        return ch - '0';
    }
}

struct Operator final {
    TokenType op;
    // 0 - , ( )
    // 1 - ?:
    // 2 - ||
    // 3 - &&
    // 4 - |
    // 5 - ^
    // 6 - &
    // 7 - == !=
    // 8 - < <= > >=
    // 9 - << >>
    // 10 - + - (binary)
    // 11 - * / %
    // 12 - + - ! ~ (unary)
    uint32_t priority;

    bool is_unary() const { return priority == 12; }

    static Operator from_token(const Token &token, bool prev_is_number) {
        switch (token.type) {
            case TokenType::eAdd:
            case TokenType::eSub:
                return {token.type, prev_is_number ? 10u : 12u};
            case TokenType::eBNot:
            case TokenType::eLNot:
                return {token.type, 12u};
            case TokenType::eMul:
            case TokenType::eDiv:
            case TokenType::eMod:
                return {token.type, 11u};
            case TokenType::eBShl:
            case TokenType::eBShr:
                return {token.type, 9u};
            case TokenType::eLess:
            case TokenType::eLessEq:
            case TokenType::eGreater:
            case TokenType::eGreaterEq:
                return {token.type, 8u};
            case TokenType::eEq:
            case TokenType::eNotEq:
                return {token.type, 7u};
            case TokenType::eBAnd:
                return {token.type, 6u};
            case TokenType::eBXor:
                return {token.type, 5u};
            case TokenType::eBOr:
                return {token.type, 4u};
            case TokenType::eLAnd:
                return {token.type, 3u};
            case TokenType::eLOr:
                return {token.type, 2u};
            case TokenType::eQuestion:
            case TokenType::eColon:
                return {token.type, 1u};
            case TokenType::eComma:
                return {token.type, 0u};
            case TokenType::eLeftBracketRound:
            case TokenType::eRightBracketRound:
            case TokenType::eEof: // treat eof as )
                return {token.type, 0u};
            default:
                throw EvaluateError{concat("operator '", token.value, "' not allowed here")};
        }
    }
};

int64_t do_unary_op(TokenType op, int64_t a) {
    switch (op) {
        case TokenType::eAdd: return a;
        case TokenType::eSub: return -a;
        case TokenType::eBNot: return ~a;
        case TokenType::eLNot: return a ? 0 : 1;
        default: unreachable();
    }
}

int64_t do_binary_op(TokenType op, int64_t a, int64_t b) {
    switch (op) {
        case TokenType::eAdd: return a + b;
        case TokenType::eSub: return a - b;
        case TokenType::eMul: return a * b;
        case TokenType::eDiv: return a / b;
        case TokenType::eMod: return a % b;
        case TokenType::eBShl: return a << b;
        case TokenType::eBShr: return a >> b;
        case TokenType::eLess: return a < b;
        case TokenType::eLessEq: return a <= b;
        case TokenType::eGreater: return a > b;
        case TokenType::eGreaterEq: return a >= b;
        case TokenType::eEq: return a == b;
        case TokenType::eNotEq: return a != b;
        case TokenType::eBAnd: return a & b;
        case TokenType::eBXor: return a ^ b;
        case TokenType::eBOr: return a | b;
        case TokenType::eLAnd: return a && b;
        case TokenType::eLOr: return a || b;
        default: unreachable();
    }
}

}

int64_t str_to_number(std::string_view str) {
    int64_t base = 10;
    auto is_floating = str.find('.') != std::string_view::npos
        || (!str.starts_with("0x") && str.find_first_of("eE") != std::string_view::npos)
        || (str.starts_with("0x") && str.find_first_of("pP") != std::string_view::npos);
    if (is_floating) {
        throw EvaluateError{"floating point literal in preprocessor expression."};
    }
    size_t i = 0;
    if (str[0] == '0') {
        if (str.size() == 1) { return 0; }
        if (str[1] == 'x') {
            base = 16;
            i = 2;
        } else if (str[1] == 'b') {
            base = 2;
            i = 2;
        } else if (std::isdigit(str[1])) {
            base = 8;
            i = 2;
        } else if (str[1] == '\'' && std::isdigit(str[2])) {
            base = 8;
            i = 3;
        } else {
            i = 1;
        }
    }
    int64_t value = 0.0;
    for (; i < str.size(); i++) {
        if (str[i] == '\'') { continue; }
        if (!std::isdigit(str[i])) { break; }
        value = value * base + char_to_number(str[i]);
    }
    return value;
}

bool evaluate_expression(InputState &input) {
    std::vector<int64_t> values;
    std::vector<Operator> ops;
    std::stack<size_t> left_brackets;
    left_brackets.push(0);
    bool prev_is_number = false;
    size_t num_questions = 0;
    std::stack<size_t> num_questions_left_bracket;
    num_questions_left_bracket.push(0);
    size_t num_unary = 0;
    std::string temp{};
    while (true) {
        auto token = get_next_token(input, temp, false);
        if (token.type == TokenType::eNumber) {
            if (prev_is_number) {
                throw EvaluateError{"expected an operator after a number"};
            }
            auto value = str_to_number(token.value);
            while (!ops.empty() && ops.back().is_unary()) {
                value = do_unary_op(ops.back().op, value);
                ops.pop_back();
                --num_unary;
            }
            values.push_back(value);
            prev_is_number = true;
        } else {
            auto op = Operator::from_token(token, prev_is_number);
            if (op.op == TokenType::eLeftBracketRound) {
                if (prev_is_number) {
                    throw EvaluateError{"expected an operator before '('"};
                }
                left_brackets.push(values.size());
                num_questions_left_bracket.push(num_questions);
                prev_is_number = false;
            } else if (op.op == TokenType::eComma) {
                // clear stack since expressions before comma will not have any (side) effect
                if (!prev_is_number) {
                    throw EvaluateError{"expected a number or unary operator after an operator other than ')'"};
                }
                if (num_questions > num_questions_left_bracket.top()) {
                    throw EvaluateError{"'?' without a ':'"};
                }
                values.resize(left_brackets.top());
                ops.resize(left_brackets.top());
                prev_is_number = false;
            } else {
                if (!prev_is_number && !op.is_unary()) {
                    throw EvaluateError{"expected a number or unary operator after an operator other than ')'"};
                }
                if (op.op == TokenType::eQuestion) {
                    ++num_questions;
                } else if (op.op == TokenType::eColon) {
                    if (num_questions == num_questions_left_bracket.top()) {
                        throw EvaluateError{"':' before a '?'"};
                    }
                    --num_questions;
                }
                if (op.is_unary()) {
                    ++num_unary;
                    ops.push_back(op);
                    continue;
                }
                size_t start = values.size() - 1;
                // leave ternary op at last
                while (
                    start > left_brackets.top()
                    && ops[num_unary + start - 1].priority > op.priority
                    && ops[num_unary + start - 1].priority > 1
                ) {
                    --start;
                }
                for (size_t i = ops.size(), j = ops.size(); i > start + num_unary; i--) {
                    if (i - 1 == start || ops[i - 2].priority != ops[j - 1].priority) {
                        auto value = values[i - 1 - num_unary];
                        for (size_t k = i; k <= j; k++) {
                            value = do_binary_op(ops[k - 1].op, value, values[k - num_unary]);
                        }
                        j = i - 1;
                        values[j - num_unary] = value;
                    }
                }
                if (op.op == TokenType::eRightBracketRound || op.op == TokenType::eEof) {
                    if (num_questions > num_questions_left_bracket.top()) {
                        throw EvaluateError{"'?' without a ':'"};
                    }
                    // calc ternary
                    if (start > left_brackets.top()) {
                        std::stack<int64_t> ternary_values;
                        ternary_values.push(values.back());
                        for (size_t i = start; i > left_brackets.top(); i--) {
                            if (ops[num_unary + i - 1].op == TokenType::eColon) {
                                ternary_values.push(values[i - 1]);
                            } else {
                                auto t = ternary_values.top();
                                ternary_values.pop();
                                auto f = ternary_values.top();
                                ternary_values.pop();
                                ternary_values.push(values[i - 1] ? t : f);
                            }
                        }
                        values[left_brackets.top()] = ternary_values.top();
                    }
                    // calc unary
                    int64_t value = values[left_brackets.top()];
                    while (!ops.empty() && ops.back().is_unary()) {
                        value = do_unary_op(ops.back().op, value);
                        ops.pop_back();
                        --num_unary;
                    }
                    values[left_brackets.top()] = value;

                    values.resize(left_brackets.top() + 1);
                    ops.resize(left_brackets.top() + num_unary);
                    left_brackets.pop();
                    num_questions_left_bracket.pop();
                    if (op.op == TokenType::eEof) { break; }
                    // set to TRUE, '+' and '-' after ')' is add/sub instead of pos/neg
                    prev_is_number = true;
                } else {
                    values.resize(start + 1);
                    ops.resize(start + 1 + num_unary);
                    ops[start] = op;
                    prev_is_number = false;
                }
            }
        }
    }
    if (num_questions > 0) {
        throw EvaluateError{"'?' without a ':'"};
    }
    return values[0];
}

PEP_CPREP_NAMESPACE_END

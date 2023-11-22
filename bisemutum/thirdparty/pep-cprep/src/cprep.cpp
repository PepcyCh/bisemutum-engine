#include <cprep/cprep.hpp>

#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>
#include <algorithm>

#include "tokenize.hpp"
#include "evaluate.hpp"
#include "utils.hpp"

PEP_CPREP_NAMESPACE_BEGIN

namespace {

constexpr size_t MAX_ERROR_SIZE = 4096;

struct Define final {
    std::string_view replace;
    std::vector<std::string_view> params;
    bool function_like = false;
    bool has_va_params = false;
    std::string_view file;
    size_t lineno = 0;
};

struct FileState final {
    std::string_view path;
    std::string_view content;
    std::string_view included_by_path;
    size_t included_by_lineno;
};

enum class IfState {
    eTrue,
    // if...elif...elif..., no expression is true before
    eFalseWithoutTrueBefore,
    // if...elif...elif..., some expression is true before
    eFalseWithTrueBefore,
};
IfState if_state_from_bool(bool cond) { return cond ? IfState::eTrue : IfState::eFalseWithoutTrueBefore; }
IfState if_state_else(IfState state) {
    return state == IfState::eTrue ? IfState::eFalseWithTrueBefore
        : state == IfState::eFalseWithoutTrueBefore ? IfState::eTrue
        : IfState::eFalseWithTrueBefore;
}

constexpr size_t kMaxMacroExpandDepth = 512;

struct Preprocessorror final {
    std::string msg;
};

std::string_view trim_string_view(std::string_view s) {
    size_t start = 0;
    size_t end = s.size();
    while (start < end && std::isspace(s[start])) { ++start; }
    while (start < end && std::isspace(s[end - 1])) { --end; }
    return s.substr(start, end - start);
}

std::string stringify(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (auto ch : input) {
        if (ch == '"') {
            output += "\\\"";
        } else if (ch == '\\') {
            output += "\\\\";
        } else {
            output += ch;
        }
    }
    return output;
}

std::string normalize_path(std::string_view path) {
    const auto is_absolute = !path.empty() && path[0] == '/';
    std::vector<std::string_view> parts;
    size_t last_p = 0;
    for (auto p = path.find_first_of("/\\"); p != std::string::npos; p = path.find_first_of("/\\", p + 1)) {
        parts.push_back(path.substr(last_p, p - last_p));
        last_p = p + 1;
    }
    parts.push_back(path.substr(last_p));

    std::vector<size_t> selected_parts;
    selected_parts.reserve(parts.size());
    for (size_t i = 0; i < parts.size(); i++) {
        const auto &s = parts[i];
        if (s == "..") {
            if (!selected_parts.empty() && parts[selected_parts.back()] != "..") {
                selected_parts.pop_back();
            } else {
                selected_parts.push_back(i);
            }
        } else if (!s.empty() && s != ".") {
            selected_parts.push_back(i);
        }
    }

    std::string normalized_path;
    if (is_absolute && (selected_parts.empty() || parts[selected_parts[0]] != "..")) {
        normalized_path += '/';
    }
    for (auto i : selected_parts) {
        normalized_path += std::string{parts[i]} + '/';
    }
    if (!normalized_path.empty()) {
        normalized_path.pop_back();
    }
    return normalized_path;
}

}

struct Preprocessor::Impl final {
    Result do_preprocess(
        std::string_view input_path,
        std::string_view input_content,
        ShaderIncluder &includer,
        const std::string_view *options,
        size_t num_options
    ) {
        init_states(input_path, input_content);
        this->includer = &includer;
        parse_options(options, num_options);

        Result result{};
        result.parsed_result.reserve(input_content.size());
        try {
            parse_source(result);
        } catch (const Preprocessorror &e) {
            result.error += "error: " + e.msg + '\n';
        }
        clear_states();
        return result;
    }

    void init_states(std::string_view input_path, std::string_view input_content) {
        auto it = parsed_files.insert(normalize_path(input_path)).first;
        files.push({*it, input_content});
        inputs.emplace(input_content);
        if_stack.push(IfState::eTrue);
    }

    void clear_states() {
        defines.clear();
        parsed_files.clear();
        pragma_once_files.clear();
        while (!files.empty()) { files.pop(); }
        while (!inputs.empty()) { inputs.pop(); }
        while (!if_stack.empty()) { if_stack.pop(); }
        includer->clear();
    }

    void parse_options(const std::string_view *options, size_t num_options) {
        std::unordered_set<std::string_view> undefines;

        auto fetch_and_trim_option = [options](size_t i) {
            auto opt = options[i];
            auto opt_b = opt.begin();
            auto opt_e = opt.end();
            while (opt_b < opt_e && std::isblank(*opt_b)) { ++opt_b; }
            while (opt_b < opt_e && std::isblank(*(opt_e - 1))) { --opt_e; }
            return std::make_pair(opt_b, opt_e);
        };

        for (size_t i = 0; i < num_options; i++) {
            auto [opt_b, opt_e] = fetch_and_trim_option(i);
            if (opt_b + 1 >= opt_e || *opt_b != '-') { continue; }
            ++opt_b;
            if (*opt_b == 'D') {
                ++opt_b;
                if (opt_b == opt_e) {
                    ++i;
                    if (i == num_options) { continue; }
                    std::tie(opt_b, opt_e) = fetch_and_trim_option(i);
                }
                auto eq = std::find(opt_b, opt_e, '=');
                auto name = make_string_view(opt_b, eq);
                Define def{};
                if (eq == opt_e) {
                    def.replace = "";
                } else {
                    def.replace = make_string_view(eq + 1, opt_e);
                }
                defines.insert({name, def});
            } else if (*opt_b == 'U') {
                ++opt_b;
                if (opt_b == opt_e) {
                    ++i;
                    if (i == num_options) { continue; }
                    std::tie(opt_b, opt_e) = fetch_and_trim_option(i);
                }
                undefines.insert(make_string_view(opt_b, opt_e));
            }
        }

        for (auto def : undefines) {
            if (auto it = defines.find(def); it != defines.end()) {
                defines.erase(it);
            }
        }
    }

    void parse_source(Result &result) {
        while (true) {
            auto token = get_next_token(
                inputs.top(), result.parsed_result, true,
                if_stack.top() == IfState::eTrue ? SpaceKeepType::eAll : SpaceKeepType::eNewLine
            );
            if (token.type == TokenType::eEof) {
                inputs.pop();
                auto top_file = files.top();
                files.pop();
                if (files.empty()) {
                    break;
                } else {
                    result.parsed_result += concat(
                        "\n#line ", top_file.included_by_lineno + 1,
                        " \"", top_file.included_by_path, "\""
                    );
                }
            } else if (token.type == TokenType::eUnknown) {
                result.parsed_result += token.value;
                add_error(result, concat(
                    "at file '", files.top().path, "' line ", inputs.top().get_lineno(),
                    ", failed to parse a valid token from '", token.value.substr(0, 15), "'"
                ));
                inputs.top().set_line_start(false);
                continue;
            }

            const auto line_start = inputs.top().at_line_start();
            inputs.top().set_line_start(false);

            if (line_start && token.type == TokenType::eSharp) {
                parse_directive(result);
            } else if (if_stack.top() == IfState::eTrue) {
                if (token.type == TokenType::eIdentifier) {
                    if (auto it = defines.find(token.value); it != defines.end()) {
                        result.parsed_result += replace_macro(token.value, it->second);
                    } else if (token.value == "__FILE__") {
                        result.parsed_result += '"';
                        result.parsed_result += files.top().path;
                        result.parsed_result += '"';
                    } else if (token.value == "__LINE__") {
                        result.parsed_result += std::to_string(inputs.top().get_lineno());
                    } else {
                        result.parsed_result += token.value;
                    }
                } else {
                    result.parsed_result += token.value;
                }
            }
        }

        if (if_stack.size() > 1) {
            throw Preprocessorror{"unterminated conditional directive"};
        }
    }

    void parse_directive(Result &result) {
        auto &input = inputs.top();
        auto token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
        if (token.type != TokenType::eIdentifier) {
            if (token.type != TokenType::eEof) {
                throw Preprocessorror{concat(
                    "at file '", files.top().path, "' line ", input.get_lineno(),
                    ", expected an identifier after '#'"
                )};
            }
            return;
        }

        bool unknown_directive = true;
        try {

            // not conditional directives
            // - error, warning
            // - pragma
            // - line
            // - define, undef
            // - include
            if (if_stack.top() == IfState::eTrue) {
                if (token.value == "error" || token.value == "warning") {
                    unknown_directive = false;
                    std::string message{};
                    while (true) {
                        token = get_next_token(input, message, false);
                        if (token.type == TokenType::eEof) {
                            break;
                        }
                        message += token.value;
                    }
                    if (token.value == "error") {
                        add_error(result, concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", ", message, "\n"
                        ));
                    } else {
                        add_warning(result, concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", ", message, "\n"
                        ));
                    }
                } else if (token.value == "pragma") {
                    unknown_directive = false;
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eIdentifier) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", expected an identifier after 'pragma'\n"
                        )};
                    }
                    if (token.value == "once") {
                        pragma_once_files.insert(files.top().path);
                    } else {
                        add_warning(result, concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", unknown pragma '", token.value, "'\n"
                        ));
                        result.parsed_result += "#pragma ";
                        result.parsed_result += token.value;
                    }
                } else if (token.value == "line") {
                    unknown_directive = false;
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eNumber) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", #line directive requires a positive integer argument\n"
                        )};
                    }
                    int64_t line;
                    try {
                        line = str_to_number(token.value);
                    } catch (const EvaluateError &e) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", #line directive requires a positive integer argument\n"
                        )};
                    }
                    if (line <= 0) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", #line directive requires a positive integer argument\n"
                        )};
                    }
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eEof) {
                        if (token.type != TokenType::eString) {
                            throw Preprocessorror{concat(
                                "at file '", files.top().path, "' line ", input.get_lineno(),
                                ", Invalid filename for #line directive\n"
                            )};
                        }
                        files.top().path = token.value.substr(1, token.value.size() - 2);
                    }
                    input.set_lineno(line - 1);
                } else if (token.value == "include") {
                    unknown_directive = false;
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    std::string_view header_name{};
                    std::string macro_replaced{};
                    InputState macro_input{macro_replaced};
                    InputState *header_input = &input;
                    bool del_is_quot = true;
                    if (token.type == TokenType::eIdentifier) {
                        if (auto it = defines.find(token.value); it != defines.end()) {
                            macro_replaced = replace_macro(token.value, it->second);
                            macro_input = InputState{macro_replaced};
                            header_input = &macro_input;
                        } else {
                            throw Preprocessorror{concat(
                                "at file '", files.top().path, "' line ", input.get_lineno(),
                                ", expected a header file name\n"
                            )};
                        }
                        token = get_next_token(*header_input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    }
                    if (token.type == TokenType::eString) {
                        header_name = token.value.substr(1, token.value.size() - 2);
                    } else if (token.type == TokenType::eLess) {
                        del_is_quot = false;
                        auto start = header_input->get_p_curr();
                        while (true) {
                            auto ch = header_input->look_next_ch();
                            if (ch == '>') {
                                header_name = make_string_view(start, header_input->get_p_curr());
                                header_input->skip_next_ch();
                                break;
                            } else if (is_eof(ch) || ch == '\n') {
                                throw Preprocessorror{concat(
                                    "at file '", files.top().path, "' line ", input.get_lineno(),
                                    ", expected a header file name\n"
                                )};
                            }
                            header_input->skip_next_ch();
                        }
                    } else {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", expected a header file name\n"
                        )};
                    }
                    ShaderIncluder::Result include_result{};
                    if (includer->require_header(header_name, files.top().path, include_result)) {
                        include_result.header_path = normalize_path(include_result.header_path);
                        if (!pragma_once_files.contains(include_result.header_path)) {
                            auto it = parsed_files.insert(std::move(include_result.header_path)).first;
                            files.push({
                                *it, include_result.header_content,
                                files.top().path, input.get_lineno(),
                            });
                            result.parsed_result += concat("#line 1 \"", *it, "\"\n");
                            inputs.emplace(include_result.header_content);
                        }
                    } else {
                        result.parsed_result += "#include ";
                        result.parsed_result += del_is_quot ? '"' : '<';
                        result.parsed_result += header_name;
                        result.parsed_result += del_is_quot ? '"' : '>';
                        add_warning(result, concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", failed to include header '", header_name, "'\n"
                        ));
                    }
                } else if (token.value == "define") {
                    unknown_directive = false;
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eIdentifier) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", expected an identifier after 'define'\n"
                        )};
                    }
                    Define macro{
                        .file = files.top().path,
                        .lineno = input.get_lineno(),
                    };
                    auto macro_name = token.value;
                    auto start = input.get_p_curr();
                    if (auto ch = input.look_next_ch(); ch == '(') {
                        input.skip_next_ch();
                        macro.function_like = true;
                        while (true) {
                            token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                            macro.has_va_params = token.type == TokenType::eTripleDots;
                            if (token.type == TokenType::eRightBracketRound) { break; }
                            if (token.type != TokenType::eIdentifier && token.type != TokenType::eTripleDots) {
                                throw Preprocessorror{concat(
                                    "at file '", files.top().path, "' line ", input.get_lineno(),
                                    ", expected an identifier or '...' when defining macro paramter\n"
                                )};
                            }
                            if (!macro.has_va_params) { macro.params.push_back(token.value); }
                            token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                            if (token.type == TokenType::eRightBracketRound) { break; }
                            if (token.type != TokenType::eComma) {
                                throw Preprocessorror{concat(
                                    "at file '", files.top().path, "' line ", input.get_lineno(),
                                    ", expected ',' or ')' after a macro paramter\n"
                                )};
                            }
                            if (macro.has_va_params) {
                                throw Preprocessorror{concat(
                                    "at file '", files.top().path, "' line ", input.get_lineno(),
                                    ", '...' must be the last macro paramter\n"
                                )};
                            }
                        }
                        start = input.get_p_curr();
                    }
                    while (true) {
                        token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                        if (token.type == TokenType::eEof) { break; }
                    }
                    macro.replace = trim_string_view(input.get_substr_to_curr(start));
                    defines.insert({macro_name, std::move(macro)});
                } else if (token.value == "undef") {
                    unknown_directive = false;
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eIdentifier) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", expected an identifier after 'undef'\n"
                        )};
                    }
                    if (auto it = defines.find(token.value); it != defines.end()) {
                        defines.erase(it);
                    }
                }
            }

            // conditional directives
            // - if, ifdef, ifndef
            // - elif, elifdef, elifndef
            // - else
            // - endif
            if (token.value == "ifdef" || token.value == "ifndef") {
                unknown_directive = false;
                if (if_stack.top() == IfState::eTrue) {
                    auto is_ifdef = token.value == "ifdef";
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eIdentifier) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", expected an identifier after '", token.value, "'\n"
                        )};
                    }
                    if_stack.push(if_state_from_bool(is_ifdef == defines.contains(token.value)));
                } else {
                    if_stack.push(IfState::eFalseWithTrueBefore);
                }
            } else if (token.value == "if") {
                unknown_directive = false;
                if (if_stack.top() == IfState::eTrue) {
                    if_stack.push(if_state_from_bool(evaluate()));
                } else {
                    if_stack.push(IfState::eFalseWithTrueBefore);
                }
            } else if (token.value == "else") {
                unknown_directive = false;
                if (if_stack.size() == 1) {
                    throw Preprocessorror{concat(
                        "at file '", files.top().path, "' line ", input.get_lineno(),
                        ", '#else' without '#if'"
                    )};
                }
                if_stack.top() = if_state_else(if_stack.top());
            } else if (token.value == "elifdef" || token.value == "elifndef") {
                if (if_stack.size() == 1) {
                unknown_directive = false;
                    throw Preprocessorror{concat(
                        "at file '", files.top().path, "' line ", input.get_lineno(),
                        ", '", token.value, "' without '#if'"
                    )};
                }
                if (if_stack.top() == IfState::eFalseWithoutTrueBefore) {
                    token = get_next_token(input, result.parsed_result, false, SpaceKeepType::eNewLine);
                    if (token.type != TokenType::eIdentifier) {
                        throw Preprocessorror{concat(
                            "at file '", files.top().path, "' line ", input.get_lineno(),
                            ", expected an identifier after '", token.value, "'\n"
                        )};
                    }
                    if_stack.top() = if_state_from_bool((token.value == "elifdef") == defines.contains(token.value));
                } else {
                    if_stack.top() = IfState::eFalseWithTrueBefore;
                }
            } else if (token.value == "elif") {
                unknown_directive = false;
                if (if_stack.size() == 1) {
                    throw Preprocessorror{concat(
                        "at file '", files.top().path, "' line ", input.get_lineno(),
                        ", '#elif' without '#if'"
                    )};
                }
                if (if_stack.top() == IfState::eFalseWithoutTrueBefore) {
                    if_stack.top() = if_state_from_bool(evaluate());
                } else {
                    if_stack.top() = IfState::eFalseWithTrueBefore;
                }
            } else if (token.value == "endif") {
                unknown_directive = false;
                if (if_stack.size() == 1) {
                    throw Preprocessorror{concat(
                        "at file '", files.top().path, "' line ", input.get_lineno(),
                        ", '#endif' without '#if'"
                    )};
                }
                if_stack.pop();
            } else if (unknown_directive && if_stack.top() == IfState::eTrue) {
                result.parsed_result += '#';
                result.parsed_result += token.value;
                add_warning(result, concat(
                    "at file '", files.top().path, "' line ", input.get_lineno(),
                    ", unknown directive '", token.value, "'"
                ));
            }
        } catch (const Preprocessorror &e) {
            add_error(result, e.msg);
        }

        // forward to line end
        const auto keep_value = unknown_directive && if_stack.top() == IfState::eTrue;
        while (true) {
            token = get_next_token(
                input, result.parsed_result, false,
                keep_value ? SpaceKeepType::eAll : SpaceKeepType::eNewLine
            );
            if (token.type == TokenType::eEof) { break; }
            if (keep_value) {
                result.parsed_result += token.value;
            }
        }
    }

    bool evaluate() {
        std::string replaced{};
        auto err_loc = concat("at file '", files.top().path, "' line ", inputs.top().get_lineno());

        // replace macro and defined()
        while (true) {
            auto token = get_next_token(inputs.top(), replaced, false);
            if (token.type == TokenType::eEof) { break; }
            if (token.type == TokenType::eUnknown) {
                throw Preprocessorror{concat(
                    err_loc, ", when evaluating expression, failed to parse a valid token from '",
                    token.value.substr(0, 15), "'"
                )};
            }
            if (token.type == TokenType::eIdentifier) {
                if (auto it = defines.find(token.value); it != defines.end()) {
                    replaced += replace_macro(token.value, it->second);
                } else if (token.value == "defined") {
                    token = get_next_token(inputs.top(), replaced, false);
                    bool value;
                    if (token.type == TokenType::eIdentifier) {
                        value = defines.contains(token.value);
                    } else {
                        if (token.type != TokenType::eLeftBracketRound) {
                            throw Preprocessorror{concat(
                                err_loc, ", expected a '(' or an identifier after 'defined'"
                            )};
                        }
                        token = get_next_token(inputs.top(), replaced, false);
                        if (token.type != TokenType::eIdentifier) {
                            throw Preprocessorror{concat(err_loc, ", expected an identifier inside 'defined'")};
                        }
                        value = defines.contains(token.value);
                        token = get_next_token(inputs.top(), replaced, false);
                        if (token.type != TokenType::eRightBracketRound) {
                            throw Preprocessorror{concat(err_loc, ", expected a ')' after 'defined'")};
                        }
                    }
                    replaced += value ? "1" : "0";
                } else {
                    replaced += token.value;
                }
            } else {
                replaced += token.value;
            }
        }

        // replace identifiers ('true', 'false', undefined)
        InputState input{replaced};
        std::string replaced2{};
        while (true) {
            auto token = get_next_token(input, replaced2, false);
            if (token.type == TokenType::eEof) { break; }
            if (token.type == TokenType::eUnknown) {
                throw Preprocessorror{concat(
                    err_loc, ", when evaluating expression, failed to parse a valid token from '",
                    token.value.substr(0, 15), "'"
                )};
            }
            if (token.type == TokenType::eIdentifier) {
                if (token.value == "true") {
                    replaced2 += "1";
                } else {
                    // both 'false' and unknown identifier are replaced with '0'
                    replaced2 += "0";
                }
            } else {
                replaced2 += token.value;
            }
        }

        // evaluate expression
        try {
            InputState input{replaced2};
            return evaluate_expression(input);
        } catch (const EvaluateError &e) {
            throw Preprocessorror{concat(err_loc, ", ", e.msg)};
        }
    }

    std::string replace_macro(
        std::string_view macro_name, const Define &macro, bool is_param = false, size_t depth = 1
    ) {
        if (depth > kMaxMacroExpandDepth) {
            return std::string{macro_name};
        }

        if (depth == 1 && !is_param) {
            curr_file = files.top().path;
            curr_line = inputs.top().get_lineno();
        }

        // parse arguments for function-like macro
        std::vector<std::string_view> args;
        std::string trailing_newlines{};
        if (macro.function_like) {
            std::string fallback{macro_name};
            args.reserve(macro.params.size());
            auto token = get_next_token(inputs.top(), fallback);
            if (token.type != TokenType::eLeftBracketRound) {
                fallback += token.value;
                return fallback;
            }
            size_t num_brackets = 0;
            auto last_begin = token.value.data() + token.value.size();
            while (true) {
                token = get_next_token(inputs.top(), fallback);
                if (token.type == TokenType::eEof) {
                    throw Preprocessorror{concat(
                        "at file '", curr_file, "' line ", curr_line,
                        ", when replacing function-like macro '", macro_name, "', "
                        "find end of input before finding corresponding ')'"
                    )};
                }
                if (token.type == TokenType::eUnknown) {
                    throw Preprocessorror{concat(
                        "at file '", curr_file, "' line ", curr_line,
                        ", when replacing function-like macro '", macro_name, "', failed to parse a valid token from '",
                        token.value.substr(0, 15), "'"
                    )};
                }
                if (token.type == TokenType::eLeftBracketRound) {
                    ++num_brackets;
                } else if (token.type == TokenType::eRightBracketRound) {
                    if (num_brackets == 0) {
                        args.push_back(trim_string_view(make_string_view(last_begin, token.value.data())));
                        break;
                    } else {
                        --num_brackets;
                    }
                } else if (token.type == TokenType::eComma) {
                    if (num_brackets == 0) {
                        args.push_back(trim_string_view(make_string_view(last_begin, token.value.data())));
                        last_begin = token.value.data() + token.value.size();
                    }
                }
            }
            if (macro.params.size() == 0 && args.size() == 1 && args[0].empty()) {
                args.pop_back();
            }
            if (macro.has_va_params) {
                if (args.size() < macro.params.size()) {
                    throw Preprocessorror{concat(
                        "at file '", curr_file, "' line ", curr_line,
                        ", when replacing function-like macro '", macro_name, "', "
                        "the macro needs at least ", macro.params.size(), " arguments but ", args.size(), " are given"
                    )};
                }
            } else {
                if (args.size() != macro.params.size()) {
                    throw Preprocessorror{concat(
                        "at file '", curr_file, "' line ", curr_line,
                        ", when replacing function-like macro '", macro_name, "', "
                        "the macro needs ", macro.params.size(), " arguments but ", args.size(), " are given"
                    )};
                }
            }
            // keep line number of input
            for (size_t i = macro_name.size(); i < fallback.size(); i++) {
                if (fallback[i] == '\n') { trailing_newlines += '\n'; }
            }
        }

        auto err_loc = concat(
            "at file '", curr_file, "' line ", curr_line,
            ", when replacing macro ", std::string_view{is_param ? "parameter" : ""}, "'", macro_name,
            "' (defined at file '", macro.file, "' line ", macro.lineno, ")"
        );
        inputs.emplace(macro.replace);

        // replace macro parameters and __VA_ARGS__, do stringification and concatenation
        auto append_token = [this, &macro, &args, depth](std::string &str, const Token &token) {
            if (macro.has_va_params && token.value == "__VA_ARGS__") {
                for (size_t i = macro.params.size(); i < args.size(); i++) {
                    if (i > macro.params.size()) {
                        str += ", ";
                    }
                    Define param{
                        .replace = args[i],
                        .file = macro.file,
                        .lineno = macro.lineno,
                    };
                    str += replace_macro(token.value, param, true, depth);
                }
            } else if (token.type == TokenType::eIdentifier) {
                if (
                    auto it = std::find(macro.params.begin(), macro.params.end(), token.value);
                    it != macro.params.end()
                ) {
                    Define param{
                        .replace = args[it - macro.params.begin()],
                        .file = macro.file,
                        .lineno = macro.lineno,
                    };
                    str += replace_macro(token.value, param, true, depth);
                } else {
                    str += token.value;
                }
            } else {
                str += token.value;
            }
        };
        std::string result_phase1{};
        auto token = get_next_token(inputs.top(), result_phase1, true, SpaceKeepType::eSpace);
        std::string spaces{};
        std::string concat_str{};
        while (true) {
            auto next_token = get_next_token(inputs.top(), spaces, true, SpaceKeepType::eSpace);
            if (token.type == TokenType::eEof) {
                inputs.pop();
                break;
            } else if (token.type == TokenType::eUnknown) {
                inputs.pop();
                throw Preprocessorror{concat(
                    err_loc, ", when replacing macro '", macro_name,
                    "', failed to parse a valid token from '", token.value.substr(0, 15), "'"
                )};
            }
            // stringify, only when macro is function-like
            if (macro.function_like) {
                if (token.type == TokenType::eSharp) {
                    if (next_token.type != TokenType::eIdentifier) {
                        inputs.pop();
                        throw Preprocessorror{concat(err_loc, ", expected a macro parameter after '#'")};
                    }
                    if (next_token.value == "__VA_ARGS__") {
                        if (!macro.has_va_params) {
                            inputs.pop();
                            throw Preprocessorror{concat(
                                err_loc,
                                ", '__VA_ARGS__' is used after '#' but macro doesn't have variable number of paramters"
                            )};
                        }
                        for (size_t i = macro.params.size(); i < args.size(); i++) {
                            result_phase1 += i == macro.params.size() ? "\"" : ", ";
                            result_phase1 += stringify(args[i]);
                        }
                        result_phase1 += '"';
                    } else {
                        auto it = std::find(macro.params.begin(), macro.params.end(), next_token.value);
                        if (it == macro.params.end()) {
                            inputs.pop();
                            throw Preprocessorror{concat(
                                err_loc, ", expected a macro parameter after '#'"
                            )};
                        }
                        result_phase1 += '"' + stringify(args[it - macro.params.begin()]) + '"';
                    }
                    token = get_next_token(inputs.top(), result_phase1, true, SpaceKeepType::eSpace);
                    spaces.clear();
                    continue;
                }
            }
            // concat
            if (next_token.type == TokenType::eDoubleSharp) {
                next_token = get_next_token(inputs.top(), spaces, true, SpaceKeepType::eSpace);
                if (concat_str.empty()) {
                    append_token(concat_str, token);
                }
                append_token(concat_str, next_token);
                token = next_token;
                spaces.clear();
                continue;
            } else if (!concat_str.empty()) {
                result_phase1 += concat_str;
                result_phase1 += spaces;
                token = next_token;
                concat_str.clear();
                spaces.clear();
                continue;
            }
            // others
            append_token(result_phase1, token);
            token = next_token;
            result_phase1 += spaces;
            spaces.clear();
        }

        // replace __VA_OPT__
        inputs.emplace(result_phase1);
        std::string result_phase2{};
        while (true) {
            token = get_next_token(inputs.top(), result_phase2);
            if (token.type == TokenType::eEof) {
                inputs.pop();
                break;
            }
            if (token.type == TokenType::eIdentifier) {
                if (macro.has_va_params && token.value == "__VA_OPT__") {
                    token = get_next_token(inputs.top(), result_phase2);
                    if (token.type == TokenType::eLeftBracketRound) {
                        size_t num_brackets = 0;
                        auto opt_true = args.size() > macro.params.size()
                            && (!args[macro.params.size()].empty() || args.size() > macro.params.size() + 1);
                        while (true) {
                            token = get_next_token(inputs.top(), result_phase2);
                            if (token.type == TokenType::eEof) {
                                break;
                            } else if (token.type == TokenType::eLeftBracketRound) {
                                ++num_brackets;
                                if (opt_true) { result_phase2 += token.value; }
                            } else if (token.type == TokenType::eRightBracketRound) {
                                if (num_brackets == 0) {
                                    break;
                                }
                                --num_brackets;
                                if (opt_true) { result_phase2 += token.value; }
                            } else {
                                if (opt_true) { result_phase2 += token.value; }
                            }
                        }
                    } else {
                        result_phase2 += token.value;
                    }
                } else {
                    result_phase2 += token.value;
                }
            } else {
                result_phase2 += token.value;
            }
        }

        // replace macro in replaced string
        inputs.emplace(result_phase2);
        std::string result{};
        while (true) {
            token = get_next_token(inputs.top(), result);
            if (token.type == TokenType::eEof) {
                inputs.pop();
                break;
            }
            if (token.type == TokenType::eIdentifier) {
                if (auto it = defines.find(token.value); it != defines.end()) {
                    result += replace_macro(token.value, it->second, false, depth + 1);
                } else {
                    result += token.value;
                }
            } else {
                result += token.value;
            }
        }

        return result + trailing_newlines;
    }

    void add_error(Result &result, std::string_view msg) {
        if (result.error.size() >= MAX_ERROR_SIZE) { return; }
        result.error += concat("error: ", msg, "\n");
    }
    void add_warning(Result &result, std::string_view msg) {
        if (result.warning.size() >= MAX_ERROR_SIZE) { return; }
        result.warning += concat("warning: ", msg, "\n");
    }

    std::unordered_map<std::string_view, Define> defines;
    std::unordered_set<std::string> parsed_files;
    std::unordered_set<std::string_view> pragma_once_files;
    std::stack<FileState> files;
    std::stack<InputState> inputs;
    std::stack<IfState> if_stack;
    ShaderIncluder *includer = nullptr;
    std::string_view curr_file; // used in replace_macro
    size_t curr_line; // used in replace_macro
};

Preprocessor::Preprocessor() {
    impl_ = new Impl{};
}

Preprocessor::~Preprocessor() {
    if (impl_) { delete impl_; }
}

Preprocessor::Preprocessor(Preprocessor &&rhs) {
    impl_ = rhs.impl_;
    rhs.impl_ = nullptr;
}

Preprocessor &Preprocessor::operator=(Preprocessor &&rhs) {
    if (impl_) { delete impl_; }
    impl_ = rhs.impl_;
    rhs.impl_ = nullptr;
    return *this;
}

Preprocessor::Result Preprocessor::do_preprocess(
    std::string_view input_path,
    std::string_view input_content,
    ShaderIncluder &includer,
    const std::string_view *options,
    size_t num_options
) {
    return impl_->do_preprocess(input_path, input_content, includer, options, num_options);
}

PEP_CPREP_NAMESPACE_END

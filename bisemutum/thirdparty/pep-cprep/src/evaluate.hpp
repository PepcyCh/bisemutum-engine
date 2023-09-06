#pragma once

#include "utils.hpp"

PEP_CPREP_NAMESPACE_BEGIN

struct EvaluateError final {
    std::string msg;
};

bool evaluate_expression(InputState &input);

int64_t str_to_number(std::string_view str);

PEP_CPREP_NAMESPACE_END

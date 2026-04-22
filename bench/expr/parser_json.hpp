#pragma once

#include "ast.hpp"

namespace exprbench {

inline ExprSpec parse_expression_json(const std::string& json) {
    std::regex expr_re(R"__JSON__("expr"\s*:\s*"([^"]+)")__JSON__");
    std::regex vars_re(R"__JSON__("variables"\s*:\s*\[([^\]]*)\])__JSON__");
    std::smatch m_expr, m_vars;

    if (!std::regex_search(json, m_expr, expr_re)) {
        throw std::runtime_error("json missing expr field");
    }
    if (!std::regex_search(json, m_vars, vars_re)) {
        throw std::runtime_error("json missing variables field");
    }

    ExprSpec spec;
    spec.expr = m_expr[1].str();

    const std::string raw_vars = m_vars[1].str();
    std::regex item_re(R"__JSON__("([^"]+)")__JSON__");
    for (std::sregex_iterator it(raw_vars.begin(), raw_vars.end(), item_re), end; it != end; ++it) {
        spec.variables.push_back((*it)[1].str());
    }
    if (spec.variables.empty()) {
        throw std::runtime_error("variables cannot be empty");
    }
    return spec;
}

} // namespace exprbench


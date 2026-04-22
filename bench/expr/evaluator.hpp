#pragma once

#include "ast.hpp"

namespace exprbench {

template <typename T>
inline T eval_node_scalar(const Program& prog, int idx, const std::vector<T>& vars) {
    const Node& n = prog.nodes[idx];
    switch (n.kind) {
    case NodeKind::Var: return vars[static_cast<size_t>(n.var_index)];
    case NodeKind::Const: return static_cast<T>(n.const_value);
    case NodeKind::Neg: return -eval_node_scalar<T>(prog, n.lhs, vars);
    case NodeKind::Add: return eval_node_scalar<T>(prog, n.lhs, vars) + eval_node_scalar<T>(prog, n.rhs, vars);
    case NodeKind::Sub: return eval_node_scalar<T>(prog, n.lhs, vars) - eval_node_scalar<T>(prog, n.rhs, vars);
    case NodeKind::Mul: return eval_node_scalar<T>(prog, n.lhs, vars) * eval_node_scalar<T>(prog, n.rhs, vars);
    case NodeKind::Div: return eval_node_scalar<T>(prog, n.lhs, vars) / eval_node_scalar<T>(prog, n.rhs, vars);
    default: return static_cast<T>(0);
    }
}

template <typename T>
std::vector<T> compute_reference(const Program& prog,
                                 const std::vector<std::vector<T>>& inputs,
                                 size_t n) {
    std::vector<T> out(n);
    std::vector<T> vars(inputs.size(), static_cast<T>(0));
    for (size_t i = 0; i < n; ++i) {
        for (size_t v = 0; v < inputs.size(); ++v) vars[v] = inputs[v][i];
        out[i] = eval_node_scalar<T>(prog, prog.root, vars);
    }
    return out;
}

} // namespace exprbench


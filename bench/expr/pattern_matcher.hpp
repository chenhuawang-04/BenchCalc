#pragma once

#include "ast.hpp"

namespace exprbench {

inline bool is_binary_arith(NodeKind k) {
    return k == NodeKind::Add || k == NodeKind::Sub || k == NodeKind::Mul || k == NodeKind::Div;
}

inline bool is_var_node(const Program& p, int id, int* out_var = nullptr) {
    const Node& n = p.nodes[id];
    if (n.kind != NodeKind::Var) return false;
    if (out_var) *out_var = n.var_index;
    return true;
}

inline bool match_classic6(const Program& p, Classic6Pattern& out) {
    out = {};
    const Node& r = p.nodes[p.root];
    if (r.kind != NodeKind::Add) return false;
    int vf = -1;
    if (!is_var_node(p, r.rhs, &vf)) return false;

    const Node& s = p.nodes[r.lhs];
    if (s.kind != NodeKind::Sub) return false;
    const Node& ab = p.nodes[s.lhs];
    if (ab.kind != NodeKind::Add) return false;
    int va = -1, vb = -1;
    if (!is_var_node(p, ab.lhs, &va) || !is_var_node(p, ab.rhs, &vb)) return false;

    const Node& mul2 = p.nodes[s.rhs];
    if (mul2.kind != NodeKind::Mul) return false;
    int ve = -1;
    if (!is_var_node(p, mul2.rhs, &ve)) return false;
    const Node& mul1 = p.nodes[mul2.lhs];
    if (mul1.kind != NodeKind::Mul) return false;
    int vc = -1, vd = -1;
    if (!is_var_node(p, mul1.lhs, &vc) || !is_var_node(p, mul1.rhs, &vd)) return false;

    std::set<int> uniq = {va, vb, vc, vd, ve, vf};
    if (uniq.size() != 6) return false;
    out.ok = true;
    out.a = va; out.b = vb; out.c = vc; out.d = vd; out.e = ve; out.f = vf;
    return true;
}

inline bool match_poly4(const Program& p, Poly4Pattern& out) {
    out = {};
    const Node& r = p.nodes[p.root];
    if (r.kind != NodeKind::Sub) return false;
    int vd = -1;
    if (!is_var_node(p, r.rhs, &vd)) return false;

    const Node& lhs = p.nodes[r.lhs];
    if (lhs.kind != NodeKind::Add) return false;
    int va = -1;
    if (!is_var_node(p, lhs.lhs, &va)) return false;
    const Node& mul = p.nodes[lhs.rhs];
    if (mul.kind != NodeKind::Mul) return false;
    int vb = -1, vc = -1;
    if (!is_var_node(p, mul.lhs, &vb) || !is_var_node(p, mul.rhs, &vc)) return false;

    std::set<int> uniq = {va, vb, vc, vd};
    if (uniq.size() != 4) return false;
    out.ok = true;
    out.a = va; out.b = vb; out.c = vc; out.d = vd;
    return true;
}

inline bool collect_add_vars_only(const Program& p, int node_id, std::vector<int>& vars) {
    const Node& n = p.nodes[node_id];
    if (n.kind == NodeKind::Var) {
        vars.push_back(n.var_index);
        return true;
    }
    if (n.kind != NodeKind::Add) return false;
    return collect_add_vars_only(p, n.lhs, vars) && collect_add_vars_only(p, n.rhs, vars);
}

inline bool match_reduction8(const Program& p, Reduction8Pattern& out) {
    out = {};
    std::vector<int> vars;
    if (!collect_add_vars_only(p, p.root, vars)) return false;
    if (vars.size() != 8) return false;
    std::set<int> uniq(vars.begin(), vars.end());
    if (uniq.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) out.v[i] = vars[i];
    out.ok = true;
    return true;
}

inline bool match_chunk_pipeline4(const Program& p, ChunkPipeline4Pattern& out) {
    out = {};
    if (p.root < 0 || p.root >= static_cast<int>(p.nodes.size())) return false;

    const Node& n3 = p.nodes[p.root];
    if (!is_binary_arith(n3.kind)) return false;
    int vd = -1;
    if (!is_var_node(p, n3.rhs, &vd)) return false;

    const Node& n2 = p.nodes[n3.lhs];
    if (!is_binary_arith(n2.kind)) return false;
    int vc = -1;
    if (!is_var_node(p, n2.rhs, &vc)) return false;

    const Node& n1 = p.nodes[n2.lhs];
    if (!is_binary_arith(n1.kind)) return false;
    int va = -1, vb = -1;
    if (!is_var_node(p, n1.lhs, &va) || !is_var_node(p, n1.rhs, &vb)) return false;

    std::set<int> uniq = {va, vb, vc, vd};
    if (uniq.size() != 4) return false;

    out.ok = true;
    out.a = va;
    out.b = vb;
    out.c = vc;
    out.d = vd;
    out.op1 = n1.kind;
    out.op2 = n2.kind;
    out.op3 = n3.kind;
    return true;
}

} // namespace exprbench

#include "bench/expr/core_ast_parser.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace exprbench;

static void test_json_and_parser() {
    const std::string json = R"({"variables":["a","b","c"],"expr":"a + b * c"})";
    ExprSpec spec = parse_expression_json(json);
    assert(spec.variables.size() == 3);
    assert(spec.variables[0] == "a");
    assert(spec.expr == "a + b * c");

    Program p = parse_program_from_json(json);
    assert(!p.nodes.empty());
    assert(p.root >= 0);
    assert(p.variables.size() == 3);
}

static void test_reference_eval() {
    const std::string json = R"({"variables":["x","y","z"],"expr":"(x + y) * z"})";
    Program p = parse_program_from_json(json);
    std::vector<std::vector<double>> in = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {10.0, 20.0, 30.0},
    };
    auto out = compute_reference<double>(p, in, 3);
    assert(out.size() == 3);
    assert(std::abs(out[0] - 50.0) < 1e-12);
    assert(std::abs(out[1] - 140.0) < 1e-12);
    assert(std::abs(out[2] - 270.0) < 1e-12);
}

static void test_pattern_matchers() {
    {
        Program p = parse_program_from_json(
            R"({"variables":["a","b","c","d","e","f"],"expr":"a + b - c * d * e + f"})");
        Classic6Pattern m{};
        assert(match_classic6(p, m));
        assert(m.ok);
    }
    {
        Program p = parse_program_from_json(
            R"({"variables":["a","b","c","d"],"expr":"a + b * c - d"})");
        Poly4Pattern m{};
        assert(match_poly4(p, m));
        assert(m.ok);
    }
    {
        Program p = parse_program_from_json(
            R"({"variables":["a","b","c","d","e","f","g","h"],"expr":"a+b+c+d+e+f+g+h"})");
        Reduction8Pattern m{};
        assert(match_reduction8(p, m));
        assert(m.ok);
    }
    {
        Program p = parse_program_from_json(
            R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})");
        ChunkPipeline4Pattern m{};
        assert(match_chunk_pipeline4(p, m));
        assert(m.ok);
        assert(m.op1 == NodeKind::Add);
        assert(m.op2 == NodeKind::Mul);
        assert(m.op3 == NodeKind::Sub);
    }
}

int main() {
    test_json_and_parser();
    test_reference_eval();
    test_pattern_matchers();
    std::cout << "expr_unit_tests passed\n";
    return 0;
}

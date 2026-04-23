#pragma once

#include "bench_types.hpp"

namespace exprbench {

inline std::vector<CaseConfig> build_cases(const std::string& preset) {
    std::vector<CaseConfig> all = {
        {"expr_classic_1m_f64", "Classic expression, 1M double",
         R"({"variables":["a","b","c","d","e","f"],"expr":"a + b - c * d * e + f"})",
         1000000, ValueType::F64, InputProfile::PositiveOnly},
        {"expr_classic_1m_f32", "Classic expression, 1M float",
         R"({"variables":["a","b","c","d","e","f"],"expr":"a + b - c * d * e + f"})",
         1000000, ValueType::F32, InputProfile::PositiveOnly},

        {"expr_poly_1m_f64", "Polynomial style, 1M double",
         R"({"variables":["a","b","c","d"],"expr":"a + b * c - d"})",
         1000000, ValueType::F64, InputProfile::SignedWide},
        {"expr_reduction_2m_f32", "Reduction style, 2M float",
         R"({"variables":["a","b","c","d","e","f","g","h"],"expr":"a+b+c+d+e+f+g+h"})",
         2000000, ValueType::F32, InputProfile::SignedWide},
        {"expr_nested_1m_f64", "Nested parentheses, 1M double",
         R"({"variables":["x","y","z","u","v"],"expr":"(x + y) * (z - u) + v / 3.0"})",
         1000000, ValueType::F64, InputProfile::SignedNoTinyDenom},

        {"expr_chunkpipe_64k_f64", "Chunk pipeline Add->Mul->Sub, 64K double (latency)",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         64000, ValueType::F64, InputProfile::SignedWide},
        {"expr_chunkpipe_1m_f64", "Chunk pipeline Add->Mul->Sub, 1M double",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         1000000, ValueType::F64, InputProfile::SignedWide},
        {"expr_chunkpipe_1m_f32", "Chunk pipeline Add->Mul->Sub, 1M float",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         1000000, ValueType::F32, InputProfile::SignedWide},
        {"expr_chunkpipe_div_1m_f64", "Chunk pipeline Mul->Add->Div, 1M double",
         R"({"variables":["a","b","c","d"],"expr":"((a * b) + c) / d"})",
         1000000, ValueType::F64, InputProfile::SignedNoTinyDenom},
        {"expr_chunkpipe_subdiv_1m_f32", "Chunk pipeline Sub->Div->Add, 1M float",
         R"({"variables":["a","b","c","d"],"expr":"((a - b) / c) + d"})",
         1000000, ValueType::F32, InputProfile::SignedNoTinyDenom},
        {"expr_chunkpipe_8m_f32", "Chunk pipeline Add->Mul->Sub, 8M float (throughput)",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         8000000, ValueType::F32, InputProfile::SignedWide},
    };

    if (preset == "quick") {
        return {
            all[0], // classic f64
            all[1], // classic f32
            all[5], // chunkpipe 64k f64
            all[7], // chunkpipe 1m f32
        };
    }
    if (preset == "mt") {
        return {
            all[6],  // chunkpipe 1m f64
            all[7],  // chunkpipe 1m f32
            all[8],  // chunkpipe div 1m f64
            all[9],  // chunkpipe subdiv 1m f32
            all[10], // chunkpipe 8m f32
        };
    }
    if (preset == "ci") {
        return {
            all[0], all[1], all[2], all[3], // generic set
            all[6], all[7], all[8], all[9], // chunk pipeline 1M dimensions
        };
    }
    return all;
}

inline RunConfig preset_run_cfg(const std::string& preset, bool strict) {
    RunConfig c;
    if (preset == "quick") {
        c.warmup = strict ? 2 : 1;
        c.iterations = strict ? 8 : 5;
        c.target_ms = strict ? 50.0 : 25.0;
        c.max_repeat = 16;
        c.randomize_method_order = true;
        return c;
    }
    if (preset == "mt") {
        c.warmup = strict ? 4 : 3;
        c.iterations = strict ? 24 : 18;
        c.target_ms = strict ? 220.0 : 170.0;
        c.max_repeat = strict ? 80 : 64;
        c.randomize_method_order = true;
        return c;
    }
    if (preset == "ci") {
        c.warmup = strict ? 4 : 3;
        c.iterations = strict ? 25 : 18;
        c.target_ms = strict ? 220.0 : 160.0;
        c.max_repeat = strict ? 80 : 64;
        c.randomize_method_order = true;
        return c;
    }
    c.warmup = strict ? 5 : 4;
    c.iterations = strict ? 30 : 22;
    c.target_ms = strict ? 260.0 : 200.0;
    c.max_repeat = strict ? 96 : 72;
    c.randomize_method_order = true;
    return c;
}

inline const char* to_string(InputProfile p) {
    switch (p) {
    case InputProfile::PositiveOnly: return "positive";
    case InputProfile::SignedWide: return "signed";
    case InputProfile::SignedNoTinyDenom: return "signed_no_tiny_denom";
    default: return "unknown";
    }
}

} // namespace exprbench


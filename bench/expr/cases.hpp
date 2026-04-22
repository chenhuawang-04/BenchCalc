#pragma once

#include "bench_types.hpp"

namespace exprbench {

inline std::vector<CaseConfig> build_cases(const std::string& preset) {
    std::vector<CaseConfig> all = {
        {"expr_classic_1m_f64", "JSON公式 classic 6变量，1M double",
         R"({"variables":["a","b","c","d","e","f"],"expr":"a + b - c * d * e + f"})",
         1000000, ValueType::F64, InputProfile::PositiveOnly},
        {"expr_classic_1m_f32", "JSON公式 classic 6变量，1M float",
         R"({"variables":["a","b","c","d","e","f"],"expr":"a + b - c * d * e + f"})",
         1000000, ValueType::F32, InputProfile::PositiveOnly},

        {"expr_poly_1m_f64", "JSON公式 多项式，1M double",
         R"({"variables":["a","b","c","d"],"expr":"a + b * c - d"})",
         1000000, ValueType::F64, InputProfile::SignedWide},
        {"expr_reduction_2m_f32", "JSON公式 归约风格，2M float",
         R"({"variables":["a","b","c","d","e","f","g","h"],"expr":"a+b+c+d+e+f+g+h"})",
         2000000, ValueType::F32, InputProfile::SignedWide},
        {"expr_nested_1m_f64", "JSON公式 嵌套括号，1M double",
         R"({"variables":["x","y","z","u","v"],"expr":"(x + y) * (z - u) + v / 3.0"})",
         1000000, ValueType::F64, InputProfile::SignedNoTinyDenom},

        // Chunk pipeline 专项：三段操作符可不同，写回后者
        {"expr_chunkpipe_64k_f64", "Chunk流水线 Add->Mul->Sub，64K double（延迟）",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         64000, ValueType::F64, InputProfile::SignedWide},
        {"expr_chunkpipe_1m_f64", "Chunk流水线 Add->Mul->Sub，1M double（主场景）",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         1000000, ValueType::F64, InputProfile::SignedWide},
        {"expr_chunkpipe_1m_f32", "Chunk流水线 Add->Mul->Sub，1M float（主场景）",
         R"({"variables":["a","b","c","d"],"expr":"((a + b) * c) - d"})",
         1000000, ValueType::F32, InputProfile::SignedWide},
        {"expr_chunkpipe_div_1m_f64", "Chunk流水线 Mul->Add->Div，1M double（除法压力）",
         R"({"variables":["a","b","c","d"],"expr":"((a * b) + c) / d"})",
         1000000, ValueType::F64, InputProfile::SignedNoTinyDenom},
        {"expr_chunkpipe_subdiv_1m_f32", "Chunk流水线 Sub->Div->Add，1M float（除法+符号）",
         R"({"variables":["a","b","c","d"],"expr":"((a - b) / c) + d"})",
         1000000, ValueType::F32, InputProfile::SignedNoTinyDenom},
        {"expr_chunkpipe_8m_f32", "Chunk流水线 Add->Mul->Sub，8M float（吞吐）",
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
    if (preset == "ci") {
        return {
            all[0], all[1], all[2], all[3], // 通用场景
            all[6], all[7], all[8], all[9], // chunk pipeline 1M 维度
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
        return c;
    }
    if (preset == "ci") {
        c.warmup = strict ? 2 : 1;
        c.iterations = strict ? 10 : 8;
        c.target_ms = strict ? 55.0 : 35.0;
        c.max_repeat = 16;
        return c;
    }
    c.warmup = strict ? 3 : 2;
    c.iterations = strict ? 14 : 10;
    c.target_ms = strict ? 60.0 : 45.0;
    c.max_repeat = 20;
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


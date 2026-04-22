#pragma once

#include "core_ast_parser.hpp"

namespace exprbench {

enum class InputProfile {
    PositiveOnly,       // [1, 100]
    SignedWide,         // [-100, 100]
    SignedNoTinyDenom,  // [-100, 100], avoid |x| < eps
};

template <typename T>
struct MethodResult {
    std::string method;
    bool available = false;
    std::string reason;
    bool correct = false;
    double max_abs_err = 0.0;
    double max_rel_err = 0.0;
    double prepare_ms = 0.0;
    double cold_run_ms = 0.0;
    int repeat = 0;
    TimingStats stats;
    double gelem_per_s = 0.0;
    double gflops = 0.0;
};

struct CaseConfig {
    std::string id;
    std::string description;
    std::string json_expr;
    size_t data_size = 0;
    ValueType type = ValueType::F64;
    InputProfile input_profile = InputProfile::PositiveOnly;
};

struct RunConfig {
    int warmup = 2;
    int iterations = 10;
    double target_ms = 40.0;
    int max_repeat = 16;
    uint32_t seed = 42;
};

struct CliOptions {
    std::string preset = "quick"; // quick|ci|full
    std::optional<std::string> case_filter;
    std::optional<std::string> csv_path;
    bool strict = false;
    double strict_cv_threshold = 20.0;
    std::optional<int> threads;
    std::optional<uint32_t> seed;
};

} // namespace exprbench

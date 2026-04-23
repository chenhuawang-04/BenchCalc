#pragma once

#include "bench_types.hpp"

namespace exprbench {

inline bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

inline CliOptions parse_args(int argc, char** argv) {
    CliOptions o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto read = [&](const std::string& f) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + f);
            return argv[++i];
        };
        if (a == "--preset") o.preset = read("--preset");
        else if (starts_with(a, "--preset=")) o.preset = a.substr(9);
        else if (a == "--case") o.case_filter = read("--case");
        else if (starts_with(a, "--case=")) o.case_filter = a.substr(7);
        else if (a == "--csv") o.csv_path = read("--csv");
        else if (starts_with(a, "--csv=")) o.csv_path = a.substr(6);
        else if (a == "--strict") o.strict = true;
        else if (a == "--strict-cv-threshold") o.strict_cv_threshold = std::stod(read("--strict-cv-threshold"));
        else if (starts_with(a, "--strict-cv-threshold=")) o.strict_cv_threshold = std::stod(a.substr(22));
        else if (a == "--threads") o.threads = std::stoi(read("--threads"));
        else if (starts_with(a, "--threads=")) o.threads = std::stoi(a.substr(10));
        else if (a == "--seed") o.seed = static_cast<uint32_t>(std::stoul(read("--seed")));
        else if (starts_with(a, "--seed=")) o.seed = static_cast<uint32_t>(std::stoul(a.substr(7)));
        else if (a == "--warmup") o.warmup = std::stoi(read("--warmup"));
        else if (starts_with(a, "--warmup=")) o.warmup = std::stoi(a.substr(9));
        else if (a == "--iterations") o.iterations = std::stoi(read("--iterations"));
        else if (starts_with(a, "--iterations=")) o.iterations = std::stoi(a.substr(13));
        else if (a == "--target-ms") o.target_ms = std::stod(read("--target-ms"));
        else if (starts_with(a, "--target-ms=")) o.target_ms = std::stod(a.substr(12));
        else if (a == "--max-repeat") o.max_repeat = std::stoi(read("--max-repeat"));
        else if (starts_with(a, "--max-repeat=")) o.max_repeat = std::stoi(a.substr(13));
        else if (a == "--no-randomize-order") o.randomize_method_order = false;
        else if (a == "--randomize-order") o.randomize_method_order = true;
        else if (a == "--pin-cpu") o.pin_cpu = true;
        else if (a == "--high-priority") o.high_priority = true;
        else if (a == "--help" || a == "-h") {
            std::cout << "expr_method_bench --preset quick|mt|ci|full [--case key] [--csv out.csv] [--strict] "
                         "[--strict-cv-threshold x] [--threads n] [--seed n] "
                         "[--warmup n] [--iterations n] [--target-ms x] [--max-repeat n] "
                         "[--randomize-order|--no-randomize-order] [--pin-cpu] [--high-priority]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown arg: " + a);
        }
    }
    if (o.strict_cv_threshold <= 0.0 || !std::isfinite(o.strict_cv_threshold)) {
        throw std::runtime_error("strict-cv-threshold must be finite and > 0");
    }
    if (o.preset != "quick" && o.preset != "mt" && o.preset != "ci" && o.preset != "full") {
        throw std::runtime_error("preset must be quick|mt|ci|full");
    }
    if (o.threads && *o.threads <= 0) {
        throw std::runtime_error("threads must be > 0");
    }
    if (o.warmup && *o.warmup < 0) {
        throw std::runtime_error("warmup must be >= 0");
    }
    if (o.iterations && *o.iterations <= 0) {
        throw std::runtime_error("iterations must be > 0");
    }
    if (o.max_repeat && *o.max_repeat <= 0) {
        throw std::runtime_error("max-repeat must be > 0");
    }
    if (o.target_ms && (!std::isfinite(*o.target_ms) || *o.target_ms <= 0.0)) {
        throw std::runtime_error("target-ms must be finite and > 0");
    }
    return o;
}

} // namespace exprbench

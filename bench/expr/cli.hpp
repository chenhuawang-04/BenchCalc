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
        else if (a == "--help" || a == "-h") {
            std::cout << "expr_method_bench --preset quick|ci|full [--case key] [--csv out.csv] [--strict] "
                         "[--strict-cv-threshold x] [--threads n] [--seed n]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown arg: " + a);
        }
    }
    if (o.strict_cv_threshold <= 0.0 || !std::isfinite(o.strict_cv_threshold)) {
        throw std::runtime_error("strict-cv-threshold must be finite and > 0");
    }
    if (o.preset != "quick" && o.preset != "ci" && o.preset != "full") {
        throw std::runtime_error("preset must be quick|ci|full");
    }
    if (o.threads && *o.threads <= 0) {
        throw std::runtime_error("threads must be > 0");
    }
    return o;
}

} // namespace exprbench

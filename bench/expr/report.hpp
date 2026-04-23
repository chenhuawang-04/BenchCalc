#pragma once

#include "bench_types.hpp"
#include "cases.hpp"

namespace exprbench {

template <typename T>
void print_case_header(const CaseConfig& cc) {
    std::cout << "\n" << std::string(120, '=') << "\n";
    std::cout << cc.id << " | " << cc.description << "\n";
    std::cout << "n=" << cc.data_size
              << ", type=" << TypeTag<T>::name
              << ", input_profile=" << to_string(cc.input_profile) << "\n";
    std::cout << std::string(120, '=') << "\n";
}

template <typename T>
void print_method_table(const std::vector<MethodResult<T>>& rs) {
    double best = std::numeric_limits<double>::max();
    for (const auto& r : rs) {
        if (r.available && r.correct) best = std::min(best, r.stats.trimmed_mean_ms);
    }
    if (!std::isfinite(best)) best = 1.0;

    std::cout << std::left
              << std::setw(34) << "method"
              << std::setw(7) << "avail"
              << std::setw(8) << "ok"
              << std::setw(8) << "rep"
              << std::setw(10) << "prep"
              << std::setw(10) << "cold"
              << std::setw(11) << "e2e@1"
              << std::setw(11) << "e2e@10"
              << std::setw(11) << "trimmed"
              << std::setw(8) << "cv%"
              << std::setw(10) << "GElem/s"
              << std::setw(10) << "GFLOP/s"
              << std::setw(9) << "loss%"
              << "notes\n";
    std::cout << std::string(150, '-') << "\n";

    for (const auto& r : rs) {
        std::string note;
        double loss = 0.0;
        if (!r.available) {
            note = r.reason;
        } else if (!r.correct) {
            note = "max_abs=" + format_double(r.max_abs_err, 3) +
                   ", max_rel=" + format_double(r.max_rel_err, 3);
        } else {
            loss = (r.stats.trimmed_mean_ms - best) / best * 100.0;
        }

        std::cout << std::left
                  << std::setw(34) << r.method
                  << std::setw(7) << (r.available ? "yes" : "no")
                  << std::setw(8) << (r.correct ? "yes" : "no")
                  << std::setw(8) << r.repeat
                  << std::setw(10) << (r.available ? format_double(r.prepare_ms, 3) : "-")
                  << std::setw(10) << (r.available ? format_double(r.cold_run_ms, 3) : "-")
                  << std::setw(11) << (r.available ? format_double(r.e2e_n1_ms, 3) : "-")
                  << std::setw(11) << (r.available ? format_double(r.e2e_n10_ms, 3) : "-")
                  << std::setw(11) << (r.available ? format_double(r.stats.trimmed_mean_ms, 4) : "-")
                  << std::setw(8) << (r.available ? format_double(r.stats.cv_percent, 2) : "-")
                  << std::setw(10) << (r.available ? format_double(r.gelem_per_s, 3) : "-")
                  << std::setw(10) << (r.available ? format_double(r.gflops, 2) : "-")
                  << std::setw(9) << ((r.available && r.correct) ? format_double(loss, 2) : "-")
                  << note << "\n";
    }
}

} // namespace exprbench

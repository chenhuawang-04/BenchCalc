#include "expr/runner.hpp"
#include "expr/system_tuning.hpp"

int main(int argc, char** argv) {
    using namespace exprbench;
    try {
        const CliOptions opt = parse_args(argc, argv);
        auto cases = build_cases(opt.preset);
        if (opt.case_filter && !opt.case_filter->empty()) {
            std::vector<CaseConfig> filtered;
            for (const auto& c : cases) {
                if (c.id.find(*opt.case_filter) != std::string::npos) filtered.push_back(c);
            }
            cases = std::move(filtered);
        }
        if (cases.empty()) {
            std::cerr << "no cases matched\n";
            return 1;
        }

        RunConfig run_cfg = preset_run_cfg(opt.preset, opt.strict);
        if (opt.seed) run_cfg.seed = *opt.seed;
        if (opt.warmup) run_cfg.warmup = *opt.warmup;
        if (opt.iterations) run_cfg.iterations = *opt.iterations;
        if (opt.target_ms) run_cfg.target_ms = *opt.target_ms;
        if (opt.max_repeat) run_cfg.max_repeat = *opt.max_repeat;
        run_cfg.randomize_method_order = opt.randomize_method_order;

        const int hw = static_cast<int>(std::thread::hardware_concurrency());
        const int default_threads = std::max(1, std::min(8, hw));
        const int threads = opt.threads ? *opt.threads : default_threads;

        bool pin_ok = false;
        bool priority_ok = false;
        if (opt.pin_cpu) pin_ok = apply_cpu_pin_best_effort(threads);
        if (opt.high_priority) priority_ok = apply_high_priority_best_effort();

        std::cout << "========================================================================\n";
        std::cout << "Expression Engine Bench\n";
        std::cout << "========================================================================\n";
        std::cout << "preset=" << opt.preset
                  << ", strict=" << (opt.strict ? "on" : "off")
                  << ", strict_cv_th=" << opt.strict_cv_threshold
                  << ", warmup=" << run_cfg.warmup
                  << ", iterations=" << run_cfg.iterations
                  << ", target_ms=" << run_cfg.target_ms
                  << ", max_repeat=" << run_cfg.max_repeat
                  << ", seed=" << run_cfg.seed
                  << ", threads=" << threads
                  << ", randomize_order=" << (run_cfg.randomize_method_order ? "on" : "off")
                  << ", pin_cpu=" << (opt.pin_cpu ? (pin_ok ? "ok" : "failed") : "off")
                  << ", high_priority=" << (opt.high_priority ? (priority_ok ? "ok" : "failed") : "off")
                  << "\n";
        std::cout << "running cases=" << cases.size() << "\n";

        struct CsvRow {
            std::string case_id;
            std::string input_profile;
            std::string method;
            std::string type_name;
            bool available = false;
            bool correct = false;
            int repeat = 0;
            int threads = 0;
            uint32_t seed = 0;
            int warmup = 0;
            int iterations = 0;
            double target_ms = 0;
            int max_repeat = 0;
            int randomize_order = 0;
            int pin_cpu = 0;
            int high_priority = 0;
            double prepare_ms = 0;
            double cold_run_ms = 0;
            double e2e_n1_ms = 0;
            double e2e_n10_ms = 0;
            double e2e_n100_ms = 0;
            double trimmed = 0;
            double cv = 0;
            double ci95_low_ms = 0;
            double ci95_high_ms = 0;
            double ci95_half_ms = 0;
            double gelem_per_s = 0;
            double gflops = 0;
            double max_abs = 0;
            double max_rel = 0;
            std::string reason;
        };
        std::vector<CsvRow> csv_rows;

        int fail = 0;
        for (const auto& cc : cases) {
            if (cc.type == ValueType::F32) {
                print_case_header<float>(cc);
                auto rs = run_case_typed<float>(cc, run_cfg, threads);
                print_method_table(rs);
                for (const auto& r : rs) {
                    csv_rows.push_back({cc.id, to_string(cc.input_profile), r.method, "float", r.available, r.correct, r.repeat,
                                        threads, run_cfg.seed, run_cfg.warmup, run_cfg.iterations, run_cfg.target_ms,
                                        run_cfg.max_repeat, run_cfg.randomize_method_order ? 1 : 0,
                                        opt.pin_cpu ? 1 : 0, opt.high_priority ? 1 : 0,
                                        r.prepare_ms, r.cold_run_ms, r.e2e_n1_ms, r.e2e_n10_ms, r.e2e_n100_ms,
                                        r.stats.trimmed_mean_ms, r.stats.cv_percent, r.stats.ci95_low_ms,
                                        r.stats.ci95_high_ms, r.stats.ci95_half_ms,
                                        r.gelem_per_s, r.gflops, r.max_abs_err, r.max_rel_err, r.reason});
                    if (opt.strict && r.available &&
                        (!r.correct || (r.correct && r.stats.cv_percent > opt.strict_cv_threshold))) {
                        ++fail;
                    }
                }
            } else {
                print_case_header<double>(cc);
                auto rs = run_case_typed<double>(cc, run_cfg, threads);
                print_method_table(rs);
                for (const auto& r : rs) {
                    csv_rows.push_back({cc.id, to_string(cc.input_profile), r.method, "double", r.available, r.correct, r.repeat,
                                        threads, run_cfg.seed, run_cfg.warmup, run_cfg.iterations, run_cfg.target_ms,
                                        run_cfg.max_repeat, run_cfg.randomize_method_order ? 1 : 0,
                                        opt.pin_cpu ? 1 : 0, opt.high_priority ? 1 : 0,
                                        r.prepare_ms, r.cold_run_ms, r.e2e_n1_ms, r.e2e_n10_ms, r.e2e_n100_ms,
                                        r.stats.trimmed_mean_ms, r.stats.cv_percent, r.stats.ci95_low_ms,
                                        r.stats.ci95_high_ms, r.stats.ci95_half_ms,
                                        r.gelem_per_s, r.gflops, r.max_abs_err, r.max_rel_err, r.reason});
                    if (opt.strict && r.available &&
                        (!r.correct || (r.correct && r.stats.cv_percent > opt.strict_cv_threshold))) {
                        ++fail;
                    }
                }
            }
        }

        if (opt.csv_path) {
            std::ofstream ofs(*opt.csv_path, std::ios::binary | std::ios::trunc);
            ofs << "\xEF\xBB\xBF";
            ofs << "case_id,input_profile,method,type,available,correct,repeat,threads,seed,warmup,iterations,target_ms,max_repeat,"
                   "randomize_order,pin_cpu,high_priority,prepare_ms,cold_run_ms,e2e_n1_ms,e2e_n10_ms,e2e_n100_ms,"
                   "trimmed_mean_ms,cv_percent,ci95_low_ms,ci95_high_ms,ci95_half_ms,gelem_per_s,gflops,max_abs_err,max_rel_err,reason\n";
            for (const auto& row : csv_rows) {
                ofs << csv_escape(row.case_id) << ","
                    << row.input_profile << ","
                    << csv_escape(row.method) << ","
                    << row.type_name << ","
                    << (row.available ? 1 : 0) << ","
                    << (row.correct ? 1 : 0) << ","
                    << row.repeat << ","
                    << row.threads << ","
                    << row.seed << ","
                    << row.warmup << ","
                    << row.iterations << ","
                    << row.target_ms << ","
                    << row.max_repeat << ","
                    << row.randomize_order << ","
                    << row.pin_cpu << ","
                    << row.high_priority << ","
                    << row.prepare_ms << ","
                    << row.cold_run_ms << ","
                    << row.e2e_n1_ms << ","
                    << row.e2e_n10_ms << ","
                    << row.e2e_n100_ms << ","
                    << row.trimmed << ","
                    << row.cv << ","
                    << row.ci95_low_ms << ","
                    << row.ci95_high_ms << ","
                    << row.ci95_half_ms << ","
                    << row.gelem_per_s << ","
                    << row.gflops << ","
                    << row.max_abs << ","
                    << row.max_rel << ","
                    << csv_escape(row.reason) << "\n";
            }
            std::cout << "\nCSV written: " << *opt.csv_path << "\n";
        }

        if (opt.strict && fail > 0) {
            std::cout << "[Strict] failed rows: " << fail << "\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }
}


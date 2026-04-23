#include "expr/runner.hpp"

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

        const RunConfig rc = preset_run_cfg(opt.preset, opt.strict);
        RunConfig run_cfg = rc;
        if (opt.seed) run_cfg.seed = *opt.seed;

        const int hw = static_cast<int>(std::thread::hardware_concurrency());
        const int default_threads = std::max(1, std::min(8, hw));
        const int threads = opt.threads ? *opt.threads : default_threads;

        std::cout << "========================================================================\n";
        std::cout << "Expression Engine Bench（图执行器/VM/GPU动态核/Chunk非JIT）\n";
        std::cout << "========================================================================\n";
        std::cout << "preset=" << opt.preset
                  << ", strict=" << (opt.strict ? "on" : "off")
                  << ", strict_cv_th=" << opt.strict_cv_threshold
                  << ", warmup=" << run_cfg.warmup
                  << ", iterations=" << run_cfg.iterations
                  << ", target_ms=" << run_cfg.target_ms
                  << ", max_repeat=" << run_cfg.max_repeat
                  << ", seed=" << run_cfg.seed
                  << ", threads=" << threads << "\n";
        std::cout << "running cases=" << cases.size() << "\n";

        struct CsvRow {
            std::string case_id;
            std::string input_profile;
            std::string method;
            std::string type_name;
            bool available = false;
            bool correct = false;
            int repeat = 0;
            double prepare_ms = 0;
            double cold_run_ms = 0;
            double e2e_n1_ms = 0;
            double e2e_n10_ms = 0;
            double e2e_n100_ms = 0;
            double trimmed = 0;
            double cv = 0;
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
                                        r.prepare_ms, r.cold_run_ms, r.e2e_n1_ms, r.e2e_n10_ms, r.e2e_n100_ms, r.stats.trimmed_mean_ms, r.stats.cv_percent,
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
                                        r.prepare_ms, r.cold_run_ms, r.e2e_n1_ms, r.e2e_n10_ms, r.e2e_n100_ms, r.stats.trimmed_mean_ms, r.stats.cv_percent,
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
            ofs << "case_id,input_profile,method,type,available,correct,repeat,prepare_ms,cold_run_ms,e2e_n1_ms,e2e_n10_ms,e2e_n100_ms,trimmed_mean_ms,cv_percent,gelem_per_s,gflops,max_abs_err,max_rel_err,reason\n";
            for (const auto& row : csv_rows) {
                ofs << csv_escape(row.case_id) << ","
                    << row.input_profile << ","
                    << csv_escape(row.method) << ","
                    << row.type_name << ","
                    << (row.available ? 1 : 0) << ","
                    << (row.correct ? 1 : 0) << ","
                    << row.repeat << ","
                    << row.prepare_ms << ","
                    << row.cold_run_ms << ","
                    << row.e2e_n1_ms << ","
                    << row.e2e_n10_ms << ","
                    << row.e2e_n100_ms << ","
                    << row.trimmed << ","
                    << row.cv << ","
                    << row.gelem_per_s << ","
                    << row.gflops << ","
                    << row.max_abs << ","
                    << row.max_rel << ","
                    << csv_escape(row.reason) << "\n";
            }
            std::cout << "\nCSV 已输出: " << *opt.csv_path << "\n";
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

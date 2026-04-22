#include "chunk_bench_core.h"

#include <fstream>
#include <iostream>
#include <optional>

namespace {

struct CliOptions {
    std::string preset = "full"; // quick|ci|full
    std::optional<std::string> case_filter;
    std::optional<std::string> csv_path;
    std::optional<int> warmup;
    std::optional<int> iterations;
    std::optional<double> target_sample_ms;
    std::optional<int> max_repeat;
    std::optional<uint32_t> seed;
    bool strict = false;
    bool list_cases = false;
    bool help = false;
};

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), s.begin());
}

std::vector<bench::CaseConfig> build_case_matrix(const std::string& preset) {
    using WK = bench::WorkloadKind;

    std::vector<bench::CaseConfig> base = {
        {"classic_100k_f64", "经典场景: 6vec/5pass, 100K, double", 100000, 6, 5, 8, WK::Classic6Vec5Pass},
        {"classic_1m_f64", "经典场景: 6vec/5pass, 1M, double", 1000000, 6, 5, 8, WK::Classic6Vec5Pass},
        {"classic_1m_f32", "经典场景: 6vec/5pass, 1M, float", 1000000, 6, 5, 4, WK::Classic6Vec5Pass},
        {"addsub_1m_f64", "中低复杂度: 3vec/2pass, 1M, double", 1000000, 3, 2, 8, WK::AddSub3Vec2Pass},
        {"poly_1m_f64", "多项式: 4vec/4pass, 1M, double", 1000000, 4, 4, 8, WK::Polynomial4Vec4Pass},
        {"reduction_500k_f64", "归约: 8vec/1pass, 500K, double", 500000, 8, 1, 8, WK::Reduction},
        {"matrix_200k_f64", "矩阵风格: 10vec/10pass, 200K, double", 200000, 10, 10, 8, WK::MatrixLike},
        {"classic_5m_f64", "大规模: 6vec/5pass, 5M, double", 5000000, 6, 5, 8, WK::Classic6Vec5Pass},
    };

    if (preset == "quick") {
        return {
            base[0], base[1], base[2], base[4]
        };
    }
    if (preset == "ci") {
        return {
            base[0], base[1], base[2], base[3], base[4], base[5]
        };
    }
    return base;
}

bench::RunConfig preset_run_config(const std::string& preset) {
    bench::RunConfig cfg{};
    if (preset == "quick") {
        cfg.warmup_iterations = 1;
        cfg.measure_iterations = 6;
        cfg.target_sample_ms = 20.0;
        cfg.max_repeat_per_sample = 12;
        cfg.seed = 42;
        return cfg;
    }
    if (preset == "ci") {
        cfg.warmup_iterations = 1;
        cfg.measure_iterations = 8;
        cfg.target_sample_ms = 35.0;
        cfg.max_repeat_per_sample = 16;
        cfg.seed = 42;
        return cfg;
    }
    cfg.warmup_iterations = 2;
    cfg.measure_iterations = 14;
    cfg.target_sample_ms = 45.0;
    cfg.max_repeat_per_sample = 16;
    cfg.seed = 42;
    return cfg;
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions opt;

    auto read_value = [&](int& i, const std::string& flag) -> std::string {
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value for " + flag);
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            opt.help = true;
            continue;
        }
        if (arg == "--strict") {
            opt.strict = true;
            continue;
        }
        if (arg == "--list-cases") {
            opt.list_cases = true;
            continue;
        }
        if (arg == "--preset") {
            opt.preset = read_value(i, "--preset");
            continue;
        }
        if (starts_with(arg, "--preset=")) {
            opt.preset = arg.substr(std::string("--preset=").size());
            continue;
        }
        if (arg == "--case") {
            opt.case_filter = read_value(i, "--case");
            continue;
        }
        if (starts_with(arg, "--case=")) {
            opt.case_filter = arg.substr(std::string("--case=").size());
            continue;
        }
        if (arg == "--csv") {
            opt.csv_path = read_value(i, "--csv");
            continue;
        }
        if (starts_with(arg, "--csv=")) {
            opt.csv_path = arg.substr(std::string("--csv=").size());
            continue;
        }
        if (arg == "--warmup") {
            opt.warmup = std::stoi(read_value(i, "--warmup"));
            continue;
        }
        if (starts_with(arg, "--warmup=")) {
            opt.warmup = std::stoi(arg.substr(std::string("--warmup=").size()));
            continue;
        }
        if (arg == "--iterations") {
            opt.iterations = std::stoi(read_value(i, "--iterations"));
            continue;
        }
        if (starts_with(arg, "--iterations=")) {
            opt.iterations = std::stoi(arg.substr(std::string("--iterations=").size()));
            continue;
        }
        if (arg == "--target-ms") {
            opt.target_sample_ms = std::stod(read_value(i, "--target-ms"));
            continue;
        }
        if (starts_with(arg, "--target-ms=")) {
            opt.target_sample_ms = std::stod(arg.substr(std::string("--target-ms=").size()));
            continue;
        }
        if (arg == "--max-repeat") {
            opt.max_repeat = std::stoi(read_value(i, "--max-repeat"));
            continue;
        }
        if (starts_with(arg, "--max-repeat=")) {
            opt.max_repeat = std::stoi(arg.substr(std::string("--max-repeat=").size()));
            continue;
        }
        if (arg == "--seed") {
            opt.seed = static_cast<uint32_t>(std::stoul(read_value(i, "--seed")));
            continue;
        }
        if (starts_with(arg, "--seed=")) {
            opt.seed = static_cast<uint32_t>(std::stoul(arg.substr(std::string("--seed=").size())));
            continue;
        }

        throw std::invalid_argument("unknown argument: " + arg);
    }

    if (opt.preset != "quick" && opt.preset != "ci" && opt.preset != "full") {
        throw std::invalid_argument("preset must be one of: quick|ci|full");
    }

    return opt;
}

void print_help() {
    std::cout << "统一 Bench 框架（块大小预测算法）\n\n"
              << "用法:\n"
              << "  chunk_bench [--preset quick|ci|full] [--case keyword]\n"
              << "              [--warmup N] [--iterations N] [--target-ms N] [--max-repeat N]\n"
              << "              [--seed N]\n"
              << "              [--csv output.csv] [--strict] [--list-cases]\n\n"
              << "说明:\n"
              << "  --preset       预设运行强度，默认 full\n"
              << "  --case         仅运行 case id 包含该关键字的用例\n"
              << "  --warmup       预热轮数，覆盖 preset\n"
              << "  --iterations   测量轮数，覆盖 preset\n"
              << "  --target-ms    单个样本目标时长(ms)，自动换算 repeat 次数\n"
              << "  --max-repeat   单个样本最大 repeat 次数上限\n"
              << "  --seed         固定随机种子，默认 42\n"
              << "  --csv          输出机器可读结果（单文件）\n"
              << "  --strict       启用质量门禁，失败时返回非0\n"
              << "  --list-cases   仅列出可运行 case\n";
}

std::vector<bench::CaseConfig> filter_cases(const std::vector<bench::CaseConfig>& input,
                                            const std::optional<std::string>& filter) {
    if (!filter.has_value() || filter->empty()) {
        return input;
    }

    std::vector<bench::CaseConfig> out;
    for (const auto& c : input) {
        if (c.id.find(*filter) != std::string::npos) {
            out.push_back(c);
        }
    }
    return out;
}

void print_case_list(const std::vector<bench::CaseConfig>& cases) {
    std::cout << "可运行 case 列表:\n";
    for (const auto& c : cases) {
        std::cout << "  - " << c.id << " | " << c.description << "\n";
    }
}

template <typename Predicate>
std::optional<bench::AlgorithmEvaluation> find_eval(const std::vector<bench::AlgorithmEvaluation>& evals,
                                                    Predicate pred) {
    for (const auto& ev : evals) {
        if (pred(ev)) return ev;
    }
    return std::nullopt;
}

void print_case_report(const bench::CaseResult& r) {
    std::cout << "\n"
              << std::string(92, '=') << "\n"
              << r.cfg.id << " | " << r.cfg.description << "\n"
              << std::string(92, '=') << "\n";

    std::cout << "配置: data_size=" << r.cfg.data_size
              << ", vectors=" << r.cfg.num_vectors
              << ", passes=" << r.cfg.num_passes
              << ", element=" << r.cfg.element_size << "B"
              << ", workload=" << bench::to_string(r.cfg.workload)
              << ", repeat/sample=" << r.repeat_per_sample << "\n";

    std::map<size_t, std::vector<std::string>> chunk_marks;
    for (const auto& p : r.predictions) {
        chunk_marks[p.chunk].push_back(p.name);
    }

    std::cout << "\nchunk 扫描结果（单位 ms，统计基于 trimmed_mean）:\n";
    std::cout << std::left
              << std::setw(8) << "chunk"
              << std::setw(12) << "trimmed"
              << std::setw(12) << "mean"
              << std::setw(12) << "median"
              << std::setw(12) << "p95"
              << std::setw(10) << "cv%"
              << "marks\n";
    std::cout << std::string(92, '-') << "\n";

    std::vector<bench::ChunkRunResult> sorted_runs = r.chunk_runs;
    std::sort(sorted_runs.begin(), sorted_runs.end(),
              [](const bench::ChunkRunResult& a, const bench::ChunkRunResult& b) {
                  return a.chunk_size < b.chunk_size;
              });

    for (const auto& run : sorted_runs) {
        std::ostringstream marks;
        if (run.chunk_size == r.best_chunk) {
            marks << "[best] ";
        }
        auto it = chunk_marks.find(run.chunk_size);
        if (it != chunk_marks.end()) {
            for (const auto& m : it->second) {
                marks << "[" << m << "] ";
            }
        }

        std::cout << std::left
                  << std::setw(8) << run.chunk_size
                  << std::setw(12) << bench::format_double(run.stats.trimmed_mean_ms, 4)
                  << std::setw(12) << bench::format_double(run.stats.mean_ms, 4)
                  << std::setw(12) << bench::format_double(run.stats.median_ms, 4)
                  << std::setw(12) << bench::format_double(run.stats.p95_ms, 4)
                  << std::setw(10) << bench::format_double(run.stats.cv_percent, 2)
                  << marks.str() << "\n";
    }

    std::cout << "\n算法评估（相对 best）:\n";
    std::cout << std::left
              << std::setw(12) << "algorithm"
              << std::setw(10) << "chunk"
              << std::setw(14) << "time(ms)"
              << std::setw(12) << "loss(%)"
              << "speedup\n";
    std::cout << std::string(58, '-') << "\n";
    for (const auto& ev : r.algorithm_eval) {
        std::cout << std::left
                  << std::setw(12) << ev.name
                  << std::setw(10) << ev.chunk
                  << std::setw(14) << bench::format_double(ev.mean_ms, 4)
                  << std::setw(12) << bench::format_double(ev.loss_percent, 2)
                  << bench::format_double(ev.speedup_vs_best, 3) << "x\n";
    }

    std::cout << "\n一致性检查: "
              << (r.checksum_consistent ? "PASS" : "FAIL")
              << ", checksum_ref=" << bench::format_double(r.checksum_reference, 6) << "\n";
    std::cout << "最佳 chunk: " << r.best_chunk
              << ", trimmed_mean=" << bench::format_double(r.best_stats.trimmed_mean_ms, 4)
              << " ms, cv=" << bench::format_double(r.best_stats.cv_percent, 2) << "%\n";
}

void write_csv(const std::string& path, const std::vector<bench::CaseResult>& results) {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        throw std::runtime_error("failed to open csv file: " + path);
    }

    ofs << "\xEF\xBB\xBF";
    ofs << "row_type,case_id,description,data_size,num_vectors,num_passes,element_size,workload,repeat_per_sample,"
           "chunk,trimmed_mean_ms,mean_ms,median_ms,p95_ms,min_ms,max_ms,stdev_ms,cv_percent,"
           "is_best,algorithm,loss_percent,speedup_vs_best,checksum,checksum_consistent\n";

    for (const auto& r : results) {
        for (const auto& run : r.chunk_runs) {
            ofs << "chunk"
                << "," << bench::csv_escape(r.cfg.id)
                << "," << bench::csv_escape(r.cfg.description)
                << "," << r.cfg.data_size
                << "," << r.cfg.num_vectors
                << "," << r.cfg.num_passes
                << "," << r.cfg.element_size
                << "," << bench::csv_escape(bench::to_string(r.cfg.workload))
                << "," << r.repeat_per_sample
                << "," << run.chunk_size
                << "," << run.stats.trimmed_mean_ms
                << "," << run.stats.mean_ms
                << "," << run.stats.median_ms
                << "," << run.stats.p95_ms
                << "," << run.stats.min_ms
                << "," << run.stats.max_ms
                << "," << run.stats.stdev_ms
                << "," << run.stats.cv_percent
                << "," << ((run.chunk_size == r.best_chunk) ? 1 : 0)
                << ",,"
                << ","
                << ","
                << "," << run.checksum
                << "," << (r.checksum_consistent ? 1 : 0)
                << "\n";
        }

        for (const auto& ev : r.algorithm_eval) {
            ofs << "algorithm"
                << "," << bench::csv_escape(r.cfg.id)
                << "," << bench::csv_escape(r.cfg.description)
                << "," << r.cfg.data_size
                << "," << r.cfg.num_vectors
                << "," << r.cfg.num_passes
                << "," << r.cfg.element_size
                << "," << bench::csv_escape(bench::to_string(r.cfg.workload))
                << "," << r.repeat_per_sample
                << "," << ev.chunk
                << "," << ev.mean_ms
                << ",,,,"
                << ",,"
                << ","
                << ","
                << "," << bench::csv_escape(ev.name)
                << "," << ev.loss_percent
                << "," << ev.speedup_vs_best
                << ","
                << "," << (r.checksum_consistent ? 1 : 0)
                << "\n";
        }
    }
}

int quality_gate(const std::vector<bench::CaseResult>& results, bool strict) {
    if (results.empty()) {
        return 2;
    }

    int fail_count = 0;

    for (const auto& r : results) {
        if (!r.checksum_consistent) {
            std::cout << "[Gate] " << r.cfg.id << ": checksum 不一致\n";
            ++fail_count;
            continue;
        }

        auto smart = find_eval(r.algorithm_eval, [](const auto& e) { return e.name == "smart"; });
        if (!smart.has_value()) {
            std::cout << "[Gate] " << r.cfg.id << ": 缺少 smart 算法评估\n";
            ++fail_count;
            continue;
        }

        double smart_loss_threshold = 20.0;
        if (r.cfg.workload == bench::WorkloadKind::Classic6Vec5Pass &&
            r.cfg.data_size >= 1000000) {
            smart_loss_threshold = 25.0;
        }

        if (smart->loss_percent > smart_loss_threshold) {
            std::cout << "[Gate] " << r.cfg.id << ": smart 损失 " << smart->loss_percent
                      << "% 超过阈值 " << smart_loss_threshold << "%\n";
            ++fail_count;
        }

        const double cv_threshold = 12.0;
        if (r.best_stats.cv_percent > cv_threshold) {
            std::cout << "[Gate] " << r.cfg.id << ": best CV " << r.best_stats.cv_percent
                      << "% 超过阈值 " << cv_threshold << "%\n";
            ++fail_count;
        }
    }

    if (strict && fail_count > 0) {
        std::cout << "[Gate] 严格模式失败，失败用例数: " << fail_count << "\n";
        return 3;
    }

    if (fail_count == 0) {
        std::cout << "[Gate] 全部通过\n";
    } else {
        std::cout << "[Gate] 非严格模式，存在 " << fail_count << " 个失败项（仅告警）\n";
    }
    return 0;
}

void print_global_summary(const std::vector<bench::CaseResult>& results) {
    std::cout << "\n"
              << std::string(92, '=') << "\n"
              << "全局汇总\n"
              << std::string(92, '=') << "\n";

    std::map<std::string, std::vector<double>> algo_losses;
    double avg_best_cv = 0.0;
    int checksum_fail = 0;

    for (const auto& r : results) {
        avg_best_cv += r.best_stats.cv_percent;
        if (!r.checksum_consistent) ++checksum_fail;
        for (const auto& ev : r.algorithm_eval) {
            algo_losses[ev.name].push_back(ev.loss_percent);
        }
    }
    avg_best_cv /= static_cast<double>(results.size());

    std::cout << "case 数量: " << results.size()
              << ", checksum 失败: " << checksum_fail
              << ", best 平均 CV: " << bench::format_double(avg_best_cv, 2) << "%\n\n";

    std::cout << std::left
              << std::setw(12) << "algorithm"
              << std::setw(16) << "avg_loss(%)"
              << std::setw(14) << "max_loss(%)"
              << "count\n";
    std::cout << std::string(52, '-') << "\n";

    for (const auto& kv : algo_losses) {
        const auto& values = kv.second;
        if (values.empty()) continue;
        const double avg = std::accumulate(values.begin(), values.end(), 0.0) /
                           static_cast<double>(values.size());
        const double mx = *std::max_element(values.begin(), values.end());

        std::cout << std::left
                  << std::setw(12) << kv.first
                  << std::setw(16) << bench::format_double(avg, 2)
                  << std::setw(14) << bench::format_double(mx, 2)
                  << values.size() << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions opt = parse_args(argc, argv);
        if (opt.help) {
            print_help();
            return 0;
        }

        auto cases = build_case_matrix(opt.preset);
        if (opt.list_cases) {
            print_case_list(cases);
            return 0;
        }

        cases = filter_cases(cases, opt.case_filter);
        if (cases.empty()) {
            std::cerr << "没有匹配到任何 case。\n";
            return 1;
        }

        bench::RunConfig run_cfg = preset_run_config(opt.preset);
        if (opt.warmup.has_value()) run_cfg.warmup_iterations = *opt.warmup;
        if (opt.iterations.has_value()) run_cfg.measure_iterations = *opt.iterations;
        if (opt.target_sample_ms.has_value()) run_cfg.target_sample_ms = *opt.target_sample_ms;
        if (opt.max_repeat.has_value()) run_cfg.max_repeat_per_sample = *opt.max_repeat;
        if (opt.seed.has_value()) run_cfg.seed = *opt.seed;
        if (opt.strict) {
            if (!opt.warmup.has_value()) {
                run_cfg.warmup_iterations = std::max(run_cfg.warmup_iterations, 2);
            }
            if (!opt.iterations.has_value()) {
                run_cfg.measure_iterations = std::max(run_cfg.measure_iterations, 10);
            }
            if (!opt.target_sample_ms.has_value()) {
                run_cfg.target_sample_ms = std::max(run_cfg.target_sample_ms, 50.0);
            }
        }

        std::cout << "========================================================================\n";
        std::cout << "统一 Bench 框架（严谨重构版）\n";
        std::cout << "========================================================================\n";
        std::cout << "preset=" << opt.preset
                  << ", warmup=" << run_cfg.warmup_iterations
                  << ", iterations=" << run_cfg.measure_iterations
                  << ", target_ms=" << run_cfg.target_sample_ms
                  << ", max_repeat=" << run_cfg.max_repeat_per_sample
                  << ", seed=" << run_cfg.seed
                  << ", strict=" << (opt.strict ? "on" : "off")
                  << "\n";
        std::cout << "running cases=" << cases.size() << "\n";

        std::vector<bench::CaseResult> results;
        results.reserve(cases.size());

        for (const auto& c : cases) {
            if (bench::is_float_case(c)) {
                results.push_back(bench::run_case<float>(c, run_cfg));
            } else {
                results.push_back(bench::run_case<double>(c, run_cfg));
            }
            print_case_report(results.back());
        }

        print_global_summary(results);

        if (opt.csv_path.has_value()) {
            write_csv(*opt.csv_path, results);
            std::cout << "\nCSV 已输出: " << *opt.csv_path << "\n";
        }

        return quality_gate(results, opt.strict);
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }
}

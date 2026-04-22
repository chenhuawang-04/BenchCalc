#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "adaptive_chunk_selector.h"
#include "improved_chunk_selector.h"
#include "smart_chunk_selector.h"
#include "ultimate_chunk_predictor.h"

namespace bench {

enum class WorkloadKind {
    Classic6Vec5Pass,
    AddSub3Vec2Pass,
    Polynomial4Vec4Pass,
    Reduction,
    MatrixLike
};

inline const char* to_string(WorkloadKind kind) {
    switch (kind) {
    case WorkloadKind::Classic6Vec5Pass: return "classic_6vec_5pass";
    case WorkloadKind::AddSub3Vec2Pass: return "add_sub_3vec_2pass";
    case WorkloadKind::Polynomial4Vec4Pass: return "poly_4vec_4pass";
    case WorkloadKind::Reduction: return "reduction";
    case WorkloadKind::MatrixLike: return "matrix_like";
    default: return "unknown";
    }
}

struct CaseConfig {
    std::string id;
    std::string description;
    size_t data_size = 0;
    size_t num_vectors = 0;
    size_t num_passes = 0;
    size_t element_size = 8; // float=4, double=8
    WorkloadKind workload = WorkloadKind::Classic6Vec5Pass;
};

struct RunConfig {
    int warmup_iterations = 2;
    int measure_iterations = 12;
    uint32_t seed = 42;
    bool reset_input_each_iteration = true;
    bool randomize_chunk_order = true;
    double target_sample_ms = 40.0;
    int max_repeat_per_sample = 16;
};

struct TimingStats {
    size_t samples = 0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double mean_ms = 0.0;
    double median_ms = 0.0;
    double p95_ms = 0.0;
    double stdev_ms = 0.0;
    double cv_percent = 0.0;
    double trimmed_mean_ms = 0.0;
};

struct ChunkRunResult {
    size_t chunk_size = 0;
    TimingStats stats;
    double checksum = 0.0;
};

struct AlgorithmPrediction {
    std::string name;
    size_t chunk = 0;
};

struct AlgorithmEvaluation {
    std::string name;
    size_t chunk = 0;
    double mean_ms = 0.0;
    double loss_percent = 0.0;
    double speedup_vs_best = 0.0;
};

struct CaseResult {
    CaseConfig cfg;
    std::vector<AlgorithmPrediction> predictions;
    std::vector<ChunkRunResult> chunk_runs;
    std::vector<AlgorithmEvaluation> algorithm_eval;
    int repeat_per_sample = 1;
    size_t best_chunk = 0;
    TimingStats best_stats;
    bool checksum_consistent = true;
    double checksum_reference = 0.0;
};

inline bool is_float_case(const CaseConfig& cfg) {
    return cfg.element_size == 4;
}

inline TimingStats summarize_timings(const std::vector<double>& samples_ms) {
    TimingStats s{};
    if (samples_ms.empty()) {
        return s;
    }

    s.samples = samples_ms.size();
    s.min_ms = *std::min_element(samples_ms.begin(), samples_ms.end());
    s.max_ms = *std::max_element(samples_ms.begin(), samples_ms.end());
    s.mean_ms = std::accumulate(samples_ms.begin(), samples_ms.end(), 0.0) /
                static_cast<double>(samples_ms.size());

    std::vector<double> sorted = samples_ms;
    std::sort(sorted.begin(), sorted.end());

    const size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0) {
        s.median_ms = (sorted[mid - 1] + sorted[mid]) / 2.0;
    } else {
        s.median_ms = sorted[mid];
    }

    const size_t p95_index = static_cast<size_t>(std::ceil((sorted.size() - 1) * 0.95));
    s.p95_ms = sorted[p95_index];

    double variance = 0.0;
    for (double v : samples_ms) {
        const double d = v - s.mean_ms;
        variance += d * d;
    }
    variance /= static_cast<double>(samples_ms.size());
    s.stdev_ms = std::sqrt(variance);
    s.cv_percent = (s.mean_ms > 0.0) ? (s.stdev_ms / s.mean_ms * 100.0) : 0.0;

    // Trimmed mean: 去掉一个最小值和一个最大值（样本数量足够时）
    if (sorted.size() >= 5) {
        double trimmed_sum = 0.0;
        for (size_t i = 1; i + 1 < sorted.size(); ++i) {
            trimmed_sum += sorted[i];
        }
        s.trimmed_mean_ms = trimmed_sum / static_cast<double>(sorted.size() - 2);
    } else {
        s.trimmed_mean_ms = s.mean_ms;
    }

    return s;
}

template <typename T>
class VectorScenario {
public:
    explicit VectorScenario(const CaseConfig& cfg, uint32_t seed)
        : cfg_(cfg),
          base_(cfg.num_vectors, std::vector<T>(cfg.data_size)),
          work_(cfg.num_vectors, std::vector<T>(cfg.data_size)) {
        validate_config();
        fill_random(seed);
        reset();
    }

    void reset() { work_ = base_; }

    void run_chunked(size_t chunk_size) {
        if (chunk_size == 0) {
            throw std::invalid_argument("chunk_size cannot be zero");
        }
        for (size_t block = 0; block < cfg_.data_size; block += chunk_size) {
            const size_t end = std::min(block + chunk_size, cfg_.data_size);
            execute_block(block, end);
        }
    }

    double sample_guard_value() const {
        const auto& out = work_.front();
        if (out.empty()) return 0.0;
        const size_t stride = std::max<size_t>(1, out.size() / 32);
        double acc = 0.0;
        for (size_t i = 0; i < out.size(); i += stride) {
            acc += static_cast<double>(out[i]);
        }
        return acc;
    }

    double full_checksum() const {
        const auto& out = work_.front();
        double acc = 0.0;
        for (T v : out) {
            acc += static_cast<double>(v);
        }
        return acc;
    }

private:
    void validate_config() const {
        if (cfg_.data_size == 0 || cfg_.num_vectors == 0) {
            throw std::invalid_argument("invalid case: data_size/num_vectors must be > 0");
        }

        switch (cfg_.workload) {
        case WorkloadKind::Classic6Vec5Pass:
            if (cfg_.num_vectors < 6) throw std::invalid_argument("Classic6Vec5Pass needs >=6 vectors");
            break;
        case WorkloadKind::AddSub3Vec2Pass:
            if (cfg_.num_vectors < 3) throw std::invalid_argument("AddSub needs >=3 vectors");
            break;
        case WorkloadKind::Polynomial4Vec4Pass:
            if (cfg_.num_vectors < 4) throw std::invalid_argument("Polynomial needs >=4 vectors");
            break;
        case WorkloadKind::Reduction:
            if (cfg_.num_vectors < 2) throw std::invalid_argument("Reduction needs >=2 vectors");
            break;
        case WorkloadKind::MatrixLike:
            if (cfg_.num_vectors < 2) throw std::invalid_argument("MatrixLike needs >=2 vectors");
            break;
        default:
            throw std::invalid_argument("unknown workload kind");
        }
    }

    void fill_random(uint32_t seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> dist(1.0, 100.0);
        for (auto& vec : base_) {
            for (auto& x : vec) {
                x = static_cast<T>(dist(rng));
            }
        }
    }

    void execute_block(size_t start, size_t end) {
        switch (cfg_.workload) {
        case WorkloadKind::Classic6Vec5Pass:
            // a = a + b - c * d * e + f
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] + work_[1][i];
            for (size_t i = start; i < end; ++i) work_[2][i] = work_[2][i] * work_[3][i];
            for (size_t i = start; i < end; ++i) work_[2][i] = work_[2][i] * work_[4][i];
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] - work_[2][i];
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] + work_[5][i];
            return;

        case WorkloadKind::AddSub3Vec2Pass:
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] + work_[1][i];
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] - work_[2][i];
            return;

        case WorkloadKind::Polynomial4Vec4Pass:
            // a = a + b*c - d（保留多次遍历结构）
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] + work_[1][i];
            for (size_t i = start; i < end; ++i) work_[1][i] = work_[1][i] * work_[2][i];
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] + work_[1][i];
            for (size_t i = start; i < end; ++i) work_[0][i] = work_[0][i] - work_[3][i];
            return;

        case WorkloadKind::Reduction:
            for (size_t i = start; i < end; ++i) {
                T sum = work_[0][i];
                for (size_t v = 1; v < cfg_.num_vectors; ++v) {
                    sum += work_[v][i];
                }
                work_[0][i] = sum;
            }
            return;

        case WorkloadKind::MatrixLike:
            // a = sum(v[j])，先清零再累加
            for (size_t i = start; i < end; ++i) work_[0][i] = static_cast<T>(0);
            for (size_t v = 1; v < cfg_.num_vectors; ++v) {
                for (size_t i = start; i < end; ++i) {
                    work_[0][i] = work_[0][i] + work_[v][i];
                }
            }
            return;
        }
    }

private:
    CaseConfig cfg_;
    std::vector<std::vector<T>> base_;
    std::vector<std::vector<T>> work_;
};

template <typename T>
int calibrate_repeat_per_sample(const CaseConfig& cfg,
                                const RunConfig& run_cfg,
                                size_t calibration_chunk) {
    if (run_cfg.target_sample_ms <= 0.0) {
        return 1;
    }

    VectorScenario<T> scenario(cfg, run_cfg.seed);

    if (run_cfg.reset_input_each_iteration) scenario.reset();
    scenario.run_chunked(calibration_chunk); // warmup once

    if (run_cfg.reset_input_each_iteration) scenario.reset();
    const auto start = std::chrono::high_resolution_clock::now();
    scenario.run_chunked(calibration_chunk);
    const auto finish = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = finish - start;

    const double one_run_ms = std::max(0.05, elapsed.count());
    int repeats = static_cast<int>(std::ceil(run_cfg.target_sample_ms / one_run_ms));
    repeats = std::max(1, repeats);
    repeats = std::min(run_cfg.max_repeat_per_sample, repeats);
    return repeats;
}

template <typename T>
std::vector<ChunkRunResult> benchmark_chunks_blocked(const CaseConfig& cfg,
                                                     const RunConfig& run_cfg,
                                                     const std::vector<size_t>& chunks,
                                                     int repeat_per_sample) {
    if (chunks.empty()) {
        return {};
    }

    VectorScenario<T> scenario(cfg, run_cfg.seed);
    std::vector<size_t> order = chunks;
    std::unordered_map<size_t, std::vector<double>> sample_map;
    sample_map.reserve(chunks.size());

    for (size_t c : chunks) {
        sample_map[c].reserve(static_cast<size_t>(run_cfg.measure_iterations));
    }

    std::mt19937 rng(run_cfg.seed ^ 0x9E3779B9u);
    static volatile double g_bench_sink = 0.0;

    // Warmup：每轮遍历全部 chunk，避免先后顺序偏差
    for (int i = 0; i < run_cfg.warmup_iterations; ++i) {
        if (run_cfg.randomize_chunk_order) {
            std::shuffle(order.begin(), order.end(), rng);
        }
        for (size_t chunk : order) {
            if (run_cfg.reset_input_each_iteration) scenario.reset();
            scenario.run_chunked(chunk);
            g_bench_sink += scenario.sample_guard_value();
        }
    }

    // Measured：blocked round-robin，样本更稳健
    for (int i = 0; i < run_cfg.measure_iterations; ++i) {
        if (run_cfg.randomize_chunk_order) {
            std::shuffle(order.begin(), order.end(), rng);
        }

        for (size_t chunk : order) {
            const auto start = std::chrono::high_resolution_clock::now();
            for (int rep = 0; rep < repeat_per_sample; ++rep) {
                if (run_cfg.reset_input_each_iteration) scenario.reset();
                scenario.run_chunked(chunk);
                g_bench_sink += scenario.sample_guard_value();
            }
            const auto finish = std::chrono::high_resolution_clock::now();

            const std::chrono::duration<double, std::milli> elapsed = finish - start;
            sample_map[chunk].push_back(elapsed.count() / static_cast<double>(repeat_per_sample));
        }
    }

    std::vector<ChunkRunResult> out;
    out.reserve(chunks.size());
    for (size_t chunk : chunks) {
        ChunkRunResult run;
        run.chunk_size = chunk;
        run.stats = summarize_timings(sample_map[chunk]);

        // checksum 单独验证，不计入 timing 样本
        if (run_cfg.reset_input_each_iteration) scenario.reset();
        scenario.run_chunked(chunk);
        run.checksum = scenario.full_checksum();
        g_bench_sink += scenario.sample_guard_value();

        out.push_back(run);
    }
    return out;
}

inline std::vector<AlgorithmPrediction> collect_predictions(const CaseConfig& cfg) {
    std::vector<AlgorithmPrediction> preds;
    preds.reserve(4);

    preds.push_back({"original", AdaptiveChunkSelector::conservative_predict(
                                     cfg.data_size, cfg.num_vectors, cfg.num_passes, cfg.element_size)});
    preds.push_back({"improved", ImprovedChunkSelector::predict(
                                     cfg.data_size, cfg.num_vectors, cfg.num_passes, cfg.element_size)});
    preds.push_back({"ultimate", UltimateChunkPredictor().predict(
                                     cfg.data_size, cfg.num_vectors, cfg.num_passes, cfg.element_size)});
    preds.push_back({"smart", SmartChunkSelector::predict(
                                     cfg.data_size, cfg.num_vectors, cfg.num_passes, cfg.element_size)});
    return preds;
}

inline std::vector<size_t> make_candidates(const CaseConfig& cfg,
                                           const std::vector<AlgorithmPrediction>& preds) {
    // 固定锚点 + 预测值，保证比较稳定可重复
    std::set<size_t> chunks = {
        16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512
    };

    if (cfg.data_size <= 200000) {
        chunks.insert(768);
    }
    if (cfg.data_size <= 50000) {
        chunks.insert(1024);
    }

    for (const auto& p : preds) {
        chunks.insert(std::max<size_t>(16, std::min<size_t>(2048, p.chunk)));
    }

    std::vector<size_t> out;
    out.reserve(chunks.size());
    for (size_t c : chunks) {
        if (c <= cfg.data_size) {
            out.push_back(c);
        }
    }
    return out;
}

inline bool nearly_equal(double a, double b, double rel_tol = 1e-8, double abs_tol = 1e-6) {
    const double diff = std::abs(a - b);
    if (diff <= abs_tol) return true;
    return diff <= rel_tol * std::max(std::abs(a), std::abs(b));
}

template <typename T>
CaseResult run_case(const CaseConfig& cfg, const RunConfig& run_cfg) {
    CaseResult result;
    result.cfg = cfg;
    result.predictions = collect_predictions(cfg);

    const auto candidates = make_candidates(cfg, result.predictions);
    const size_t calibration_chunk = std::min<size_t>(64, cfg.data_size);
    result.repeat_per_sample = calibrate_repeat_per_sample<T>(cfg, run_cfg, calibration_chunk);
    result.chunk_runs = benchmark_chunks_blocked<T>(
        cfg, run_cfg, candidates, result.repeat_per_sample
    );

    // 选取最优：trimmed_mean更稳健
    auto best_it = std::min_element(
        result.chunk_runs.begin(), result.chunk_runs.end(),
        [](const ChunkRunResult& a, const ChunkRunResult& b) {
            return a.stats.trimmed_mean_ms < b.stats.trimmed_mean_ms;
        });

    if (best_it == result.chunk_runs.end()) {
        throw std::runtime_error("no benchmark runs produced");
    }

    result.best_chunk = best_it->chunk_size;
    result.best_stats = best_it->stats;

    result.checksum_reference = result.chunk_runs.front().checksum;
    result.checksum_consistent = true;
    for (const auto& run : result.chunk_runs) {
        if (!nearly_equal(run.checksum, result.checksum_reference)) {
            result.checksum_consistent = false;
            break;
        }
    }

    // 构建 chunk -> run 的索引，便于算法评估
    std::map<size_t, const ChunkRunResult*> run_index;
    for (const auto& run : result.chunk_runs) {
        run_index[run.chunk_size] = &run;
    }

    result.algorithm_eval.reserve(result.predictions.size());
    for (const auto& pred : result.predictions) {
        auto it = run_index.find(pred.chunk);
        if (it == run_index.end()) {
            continue;
        }

        AlgorithmEvaluation ev;
        ev.name = pred.name;
        ev.chunk = pred.chunk;
        ev.mean_ms = it->second->stats.trimmed_mean_ms;
        ev.loss_percent = (ev.mean_ms - result.best_stats.trimmed_mean_ms) /
                          result.best_stats.trimmed_mean_ms * 100.0;
        ev.speedup_vs_best = result.best_stats.trimmed_mean_ms / ev.mean_ms;
        result.algorithm_eval.push_back(ev);
    }

    return result;
}

inline std::string format_double(double x, int precision = 4) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << x;
    return oss.str();
}

inline std::string csv_escape(const std::string& text) {
    bool need_quote = false;
    for (char c : text) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            need_quote = true;
            break;
        }
    }
    if (!need_quote) return text;

    std::string out = "\"";
    for (char c : text) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out += "\"";
    return out;
}

} // namespace bench

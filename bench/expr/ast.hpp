#pragma once

#include "prelude.hpp"

namespace exprbench {

enum class ValueType { F32, F64 };

template <typename T>
struct TypeTag;
template <>
struct TypeTag<float> {
    static constexpr ValueType value = ValueType::F32;
    static constexpr const char* name = "float";
};
template <>
struct TypeTag<double> {
    static constexpr ValueType value = ValueType::F64;
    static constexpr const char* name = "double";
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
    double ci95_low_ms = 0.0;
    double ci95_high_ms = 0.0;
    double ci95_half_ms = 0.0;
};

inline TimingStats summarize_timings(const std::vector<double>& samples_ms) {
    TimingStats s{};
    if (samples_ms.empty()) return s;

    s.samples = samples_ms.size();
    s.min_ms = *std::min_element(samples_ms.begin(), samples_ms.end());
    s.max_ms = *std::max_element(samples_ms.begin(), samples_ms.end());
    s.mean_ms = std::accumulate(samples_ms.begin(), samples_ms.end(), 0.0) /
                static_cast<double>(samples_ms.size());

    std::vector<double> sorted = samples_ms;
    std::sort(sorted.begin(), sorted.end());
    const size_t mid = sorted.size() / 2;
    s.median_ms = (sorted.size() % 2 == 0)
                      ? (sorted[mid - 1] + sorted[mid]) / 2.0
                      : sorted[mid];

    const size_t p95_idx = static_cast<size_t>(std::ceil((sorted.size() - 1) * 0.95));
    s.p95_ms = sorted[p95_idx];

    double var = 0.0;
    for (double v : samples_ms) {
        const double d = v - s.mean_ms;
        var += d * d;
    }
    var /= static_cast<double>(samples_ms.size());
    s.stdev_ms = std::sqrt(var);
    s.cv_percent = (s.mean_ms > 0.0) ? (s.stdev_ms / s.mean_ms * 100.0) : 0.0;

    std::vector<double> ci_samples;
    if (sorted.size() >= 5) {
        double sum = 0.0;
        for (size_t i = 1; i + 1 < sorted.size(); ++i) {
            sum += sorted[i];
            ci_samples.push_back(sorted[i]);
        }
        s.trimmed_mean_ms = sum / static_cast<double>(sorted.size() - 2);
    } else {
        s.trimmed_mean_ms = s.mean_ms;
        ci_samples = samples_ms;
    }

    if (!ci_samples.empty()) {
        const double ci_mean =
            std::accumulate(ci_samples.begin(), ci_samples.end(), 0.0) / static_cast<double>(ci_samples.size());
        double ci_var = 0.0;
        for (double v : ci_samples) {
            const double d = v - ci_mean;
            ci_var += d * d;
        }
        const double denom = static_cast<double>(std::max<size_t>(1, ci_samples.size() - 1));
        const double ci_stdev = std::sqrt(ci_var / denom);
        const double ci_stderr = ci_stdev / std::sqrt(static_cast<double>(ci_samples.size()));
        s.ci95_half_ms = 1.96 * ci_stderr;
        s.ci95_low_ms = std::max(0.0, s.trimmed_mean_ms - s.ci95_half_ms);
        s.ci95_high_ms = s.trimmed_mean_ms + s.ci95_half_ms;
    }
    return s;
}

inline std::string format_double(double x, int p = 4) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(p) << x;
    return oss.str();
}

inline std::string csv_escape(const std::string& text) {
    bool need_quote = false;
    for (char c : text) {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
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

class ParallelPool {
public:
    static ParallelPool& instance() {
        static ParallelPool p;
        return p;
    }

    void run(size_t n, int max_threads, const std::function<void(size_t, size_t)>& fn) {
        if (n == 0) return;
        const size_t min_grain = 16384;
        size_t threads = static_cast<size_t>(std::max(1, max_threads));
        threads = std::min(threads, (n + min_grain - 1) / min_grain);
        threads = std::min(threads, max_workers_ + 1);
        if (threads <= 1) {
            fn(0, n);
            return;
        }

        auto job = std::make_shared<Job>();
        job->id = ++last_job_id_;
        job->thread_count = threads;
        job->completed = 0;
        job->ranges = build_ranges(n, threads);
        job->fn = fn;

        {
            std::lock_guard<std::mutex> lk(mu_);
            current_job_ = job;
        }
        cv_.notify_all();

        const auto& r0 = job->ranges[0];
        if (r0.first < r0.second) fn(r0.first, r0.second);

        std::unique_lock<std::mutex> lk(mu_);
        cv_done_.wait(lk, [&] { return job->completed >= (job->thread_count - 1); });
        if (current_job_ && current_job_->id == job->id) current_job_.reset();
    }

private:
    struct Job {
        uint64_t id = 0;
        size_t thread_count = 1;
        std::vector<std::pair<size_t, size_t>> ranges;
        std::function<void(size_t, size_t)> fn;
        size_t completed = 0;
    };

    ParallelPool() {
        const size_t hw = std::max<unsigned>(1u, std::thread::hardware_concurrency());
        max_workers_ = (hw > 1 ? hw - 1 : 0);
        workers_.reserve(max_workers_);
        for (size_t i = 0; i < max_workers_; ++i) {
            workers_.emplace_back([this, wid = i + 1] { worker_loop(wid); });
        }
    }

    ~ParallelPool() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
            current_job_.reset();
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    static std::vector<std::pair<size_t, size_t>> build_ranges(size_t n, size_t threads) {
        std::vector<std::pair<size_t, size_t>> ranges(threads);
        size_t base = n / threads;
        size_t rem = n % threads;
        size_t begin = 0;
        for (size_t t = 0; t < threads; ++t) {
            size_t len = base + (t < rem ? 1 : 0);
            ranges[t] = {begin, begin + len};
            begin += len;
        }
        return ranges;
    }

    void worker_loop(size_t worker_id) {
        uint64_t seen = 0;
        while (true) {
            std::shared_ptr<Job> job;
            std::pair<size_t, size_t> range{0, 0};
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [&] {
                    if (stop_) return true;
                    if (!current_job_) return false;
                    if (current_job_->id == seen) return false;
                    return worker_id < current_job_->thread_count;
                });
                if (stop_) return;
                if (!current_job_) continue;
                job = current_job_;
                if (worker_id >= job->thread_count) continue;
                seen = job->id;
                range = job->ranges[worker_id];
            }

            if (range.first < range.second) {
                job->fn(range.first, range.second);
            }

            {
                std::lock_guard<std::mutex> lk(mu_);
                ++job->completed;
                if (job->completed >= (job->thread_count - 1)) {
                    cv_done_.notify_all();
                }
            }
        }
    }

private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::condition_variable cv_done_;
    std::vector<std::thread> workers_;
    size_t max_workers_ = 0;
    std::shared_ptr<Job> current_job_;
    uint64_t last_job_id_ = 0;
    bool stop_ = false;
};

inline void parallel_for(size_t n, int max_threads, const std::function<void(size_t, size_t)>& fn) {
    if (n == 0) return;
    ParallelPool::instance().run(n, max_threads, fn);
}

struct ExprSpec {
    std::vector<std::string> variables;
    std::string expr;
};

enum class NodeKind { Var, Const, Add, Sub, Mul, Div, Neg };

struct Node {
    NodeKind kind = NodeKind::Const;
    int lhs = -1;
    int rhs = -1;
    int var_index = -1;
    double const_value = 0.0;
};

struct Program {
    std::vector<Node> nodes;
    int root = -1;
    std::vector<std::string> variables;
    std::string expr_string;
};

struct Classic6Pattern {
    bool ok = false;
    int a = -1, b = -1, c = -1, d = -1, e = -1, f = -1;
};
struct Poly4Pattern {
    bool ok = false;
    int a = -1, b = -1, c = -1, d = -1;
};
struct Reduction8Pattern {
    bool ok = false;
    int v[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
};

struct ChunkPipeline4Pattern {
    bool ok = false;
    int a = -1, b = -1, c = -1, d = -1;
    NodeKind op1 = NodeKind::Add;
    NodeKind op2 = NodeKind::Add;
    NodeKind op3 = NodeKind::Add;
};

} // namespace exprbench

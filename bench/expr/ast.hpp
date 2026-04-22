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

    if (sorted.size() >= 5) {
        double sum = 0.0;
        for (size_t i = 1; i + 1 < sorted.size(); ++i) sum += sorted[i];
        s.trimmed_mean_ms = sum / static_cast<double>(sorted.size() - 2);
    } else {
        s.trimmed_mean_ms = s.mean_ms;
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

inline void parallel_for(size_t n, int max_threads, const std::function<void(size_t, size_t)>& fn) {
    if (n == 0) return;
    const size_t min_grain = 16384;
    size_t threads = static_cast<size_t>(std::max(1, max_threads));
    threads = std::min(threads, (n + min_grain - 1) / min_grain);
    if (threads <= 1) {
        fn(0, n);
        return;
    }

    std::vector<std::thread> pool;
    pool.reserve(threads - 1);
    size_t block = n / threads;
    size_t begin = 0;
    for (size_t t = 0; t + 1 < threads; ++t) {
        const size_t end = begin + block;
        pool.emplace_back([=, &fn]() { fn(begin, end); });
        begin = end;
    }
    fn(begin, n);
    for (auto& th : pool) th.join();
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

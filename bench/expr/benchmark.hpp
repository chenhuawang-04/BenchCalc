#pragma once

#include "bench_types.hpp"
#include "methods.hpp"

namespace exprbench {

template <typename T>
int calibrate_repeat(IMethod<T>& method, double target_ms, int max_repeat) {
    if (target_ms <= 0.0) return 1;
    auto st = std::chrono::high_resolution_clock::now();
    method.run_timed();
    auto ed = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> d = ed - st;
    double one = std::max(0.05, d.count());
    int r = static_cast<int>(std::ceil(target_ms / one));
    return std::max(1, std::min(max_repeat, r));
}

inline int estimate_flops_per_element(const Program& prog) {
    int flops = 0;
    for (const auto& n : prog.nodes) {
        switch (n.kind) {
        case NodeKind::Add:
        case NodeKind::Sub:
        case NodeKind::Mul:
        case NodeKind::Div:
        case NodeKind::Neg:
            ++flops;
            break;
        default:
            break;
        }
    }
    return std::max(1, flops);
}

template <typename T>
MethodResult<T> benchmark_method(IMethod<T>& method,
                                 const std::vector<T>& reference,
                                 size_t n,
                                 int flops_per_element,
                                 int warmup,
                                 int iterations,
                                 double target_ms,
                                 int max_repeat,
                                 double abs_tol,
                                 double rel_tol) {
    MethodResult<T> mr;
    mr.method = method.name();
    mr.available = method.available();
    mr.reason = method.unavailable_reason();
    if (!mr.available) return mr;

    std::vector<T> out;
    method.run_validate(out);
    if (out.size() != reference.size()) {
        mr.correct = false;
        mr.reason = "output size mismatch";
        return mr;
    }

    double max_abs = 0.0, max_rel = 0.0;
    bool all_ok = true;
    for (size_t i = 0; i < out.size(); ++i) {
        const double a = static_cast<double>(out[i]);
        const double b = static_cast<double>(reference[i]);
        const double abs_err = std::abs(a - b);
        const double rel_err = abs_err / std::max(1e-12, std::abs(b));
        max_abs = std::max(max_abs, abs_err);
        max_rel = std::max(max_rel, rel_err);
        if (abs_err > abs_tol && rel_err > rel_tol) all_ok = false;
    }
    mr.max_abs_err = max_abs;
    mr.max_rel_err = max_rel;
    mr.correct = all_ok;

    {
        auto st = std::chrono::high_resolution_clock::now();
        volatile double sink = method.run_timed();
        auto ed = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> d = ed - st;
        mr.cold_run_ms = d.count();
        (void)sink;
    }

    mr.repeat = calibrate_repeat(method, target_ms, max_repeat);

    volatile double sink = 0.0;
    for (int i = 0; i < warmup; ++i) {
        for (int r = 0; r < mr.repeat; ++r) sink += method.run_timed();
    }

    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(iterations));
    for (int i = 0; i < iterations; ++i) {
        auto st = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < mr.repeat; ++r) sink += method.run_timed();
        auto ed = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> d = ed - st;
        samples.push_back(d.count() / static_cast<double>(mr.repeat));
    }
    mr.stats = summarize_timings(samples);

    const double sec = mr.stats.trimmed_mean_ms * 1e-3;
    if (sec > 0) {
        mr.gelem_per_s = static_cast<double>(n) / sec / 1e9;
        mr.gflops = mr.gelem_per_s * static_cast<double>(flops_per_element);
    }

    mr.e2e_n1_ms = mr.prepare_ms + mr.cold_run_ms;
    mr.e2e_n10_ms = mr.prepare_ms + mr.cold_run_ms + mr.stats.trimmed_mean_ms * 9.0;
    mr.e2e_n100_ms = mr.prepare_ms + mr.cold_run_ms + mr.stats.trimmed_mean_ms * 99.0;

    (void)sink;
    return mr;
}

template <typename T>
std::vector<std::vector<T>> make_inputs(size_t vars, size_t n, uint32_t seed, InputProfile profile) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> pos(1.0, 100.0);
    std::uniform_real_distribution<double> sym(-100.0, 100.0);

    std::vector<std::vector<T>> in(vars, std::vector<T>(n));
    for (auto& v : in) {
        for (auto& x : v) {
            double raw = 0.0;
            switch (profile) {
            case InputProfile::PositiveOnly:
                raw = pos(rng);
                break;
            case InputProfile::SignedWide:
                raw = sym(rng);
                break;
            case InputProfile::SignedNoTinyDenom:
                raw = sym(rng);
                if (std::abs(raw) < 0.25) raw = (raw < 0.0 ? -0.25 : 0.25);
                break;
            }
            x = static_cast<T>(raw);
        }
    }
    return in;
}

template <typename T>
std::vector<MethodResult<T>> run_case_typed(const CaseConfig& cc, const RunConfig& rc, int threads) {
    Program prog = parse_program_from_json(cc.json_expr);
    const auto inputs = make_inputs<T>(prog.variables.size(), cc.data_size, rc.seed, cc.input_profile);
    const auto reference = compute_reference<T>(prog, inputs, cc.data_size);
    const int flops_per_element = estimate_flops_per_element(prog);

    std::vector<MethodResult<T>> out;
    out.reserve(16);

    auto run_one = [&](auto& method) {
        auto pst = std::chrono::high_resolution_clock::now();
        method.prepare(prog, inputs, cc.data_size, threads);
        auto ped = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> pd = ped - pst;

        auto mr = benchmark_method<T>(
            method, reference, cc.data_size, flops_per_element,
            rc.warmup, rc.iterations, rc.target_ms, rc.max_repeat,
            std::is_same_v<T, float> ? 2e-3 : 1e-9,
            std::is_same_v<T, float> ? 5e-5 : 1e-10);
        mr.prepare_ms = pd.count();
        mr.e2e_n1_ms = mr.prepare_ms + mr.cold_run_ms;
        mr.e2e_n10_ms = mr.prepare_ms + mr.cold_run_ms + mr.stats.trimmed_mean_ms * 9.0;
        mr.e2e_n100_ms = mr.prepare_ms + mr.cold_run_ms + mr.stats.trimmed_mean_ms * 99.0;
        out.push_back(std::move(mr));
    };

    {
        HardcodedInlineMethod<T> m;
        run_one(m);
    }
    {
        PlainLoop4Method<T> m; // 你要求的“最普通 for 循环四数组链式计算”
        run_one(m);
    }
    {
        PlainInplaceAmsMethod<T> m; // 你要求的“a[i]=... ”原地硬编码对照
        run_one(m);
    }
    {
        HardcodedInlineExactAmsMethod<T> m; // 纯内联上限：仅 ((a+b)*c)-d
        run_one(m);
    }
    {
        ChunkPipelineMethod<T> m; // 既有 chunk 方案
        run_one(m);
    }
    {
        ChunkPipelineFixed256Method<T> m; // 固定 chunk 对照
        run_one(m);
    }
    {
        ChunkPipelinePeakMethod<T> m; // 补强 chunk 方案
        run_one(m);
    }
    {
        GraphFusedMethod<T> m;
        run_one(m);
    }
    {
        VmMethod<T> m;
        run_one(m);
    }
#ifdef _WIN32
    {
        OpenCLDynamicKernelMethod<T> m(false); // GPU peak
        run_one(m);
    }
    {
        OpenCLDynamicKernelMethod<T> m(true); // GPU e2e
        run_one(m);
    }
#endif
    return out;
}

} // namespace exprbench

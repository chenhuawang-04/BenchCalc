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

    std::vector<std::function<void()>> tasks;
    tasks.reserve(12);
    tasks.push_back([&] {
        HardcodedInlineMethod<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        PlainLoop4Method<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        PlainInplaceAmsMethod<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        HardcodedInlineExactAmsMethod<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        ChunkPipelineMethod<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        ChunkPipelineFixed256Method<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        ChunkPipelinePeakMethod<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        GraphFusedMethod<T> m;
        run_one(m);
    });
    tasks.push_back([&] {
        VmMethod<T> m;
        run_one(m);
    });
#ifdef _WIN32
    tasks.push_back([&] {
        OpenCLDynamicKernelMethod<T> m(false);
        run_one(m);
    });
    tasks.push_back([&] {
        OpenCLDynamicKernelMethod<T> m(true);
        run_one(m);
    });
#endif

    if (rc.randomize_method_order) {
        std::mt19937 rng(rc.seed ^ static_cast<uint32_t>(cc.data_size) ^ static_cast<uint32_t>(threads * 131));
        std::shuffle(tasks.begin(), tasks.end(), rng);
    }
    for (auto& task : tasks) task();

    auto rank = [](const std::string& name) -> int {
        static const std::unordered_map<std::string, int> kRank = {
            {"hardcoded_inline", 10},
            {"hardcoded_plain_loop4", 20},
            {"hardcoded_plain_inplace_ams", 30},
            {"hardcoded_inline_exact_ams", 40},
            {"chunk_pipeline_nonjit", 50},
            {"chunk_pipeline_nonjit_fixed256", 60},
            {"chunk_pipeline_nonjit_peak", 70},
            {"graph_fused_kernellib", 80},
            {"vm_register", 90},
            {"gpu_dynamic_kernel_peak", 100},
            {"gpu_dynamic_kernel_e2e", 110},
        };
        auto it = kRank.find(name);
        return (it == kRank.end()) ? 9999 : it->second;
    };

    std::stable_sort(out.begin(), out.end(), [&](const auto& a, const auto& b) {
        int ra = rank(a.method), rb = rank(b.method);
        if (ra != rb) return ra < rb;
        return a.method < b.method;
    });

    return out;
}

} // namespace exprbench


#pragma once

#include "method_interface.hpp"

namespace exprbench {

template <typename T>
class ChunkPipelineMethod final : public IMethod<T> {
public:
    // 既有实现：保留不变，作为对照组
    std::string name() const override { return "chunk_pipeline_nonjit"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int threads) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        threads_ = threads;
        out_.assign(n_, T{});
        stage2_.assign(n_, T{});
        stage3_.assign(n_, T{});
        avail_ = false;
        reason_.clear();

        p4_ = {};
        if (!match_chunk_pipeline4(*prog_, p4_)) {
            reason_ = "expression not supported by chunk pipeline pattern";
            return false;
        }

        if (p4_.a < 0 || p4_.b < 0 || p4_.c < 0 || p4_.d < 0 ||
            p4_.a >= static_cast<int>(in_->size()) ||
            p4_.b >= static_cast<int>(in_->size()) ||
            p4_.c >= static_cast<int>(in_->size()) ||
            p4_.d >= static_cast<int>(in_->size())) {
            reason_ = "invalid variable mapping for chunk pipeline";
            return false;
        }

        best_chunk_ = tune_chunk();
        avail_ = true;
        return true;
    }

    bool available() const override { return avail_; }
    std::string unavailable_reason() const override { return reason_; }

    void run_validate(std::vector<T>& out) override {
        run_once(best_chunk_);
        out = out_;
    }

    double run_timed() override {
        run_once(best_chunk_);
        return sample_guard();
    }

private:
    static inline T apply(NodeKind op, T lhs, T rhs) {
        switch (op) {
        case NodeKind::Add: return lhs + rhs;
        case NodeKind::Sub: return lhs - rhs;
        case NodeKind::Mul: return lhs * rhs;
        case NodeKind::Div: return lhs / rhs;
        default: return lhs;
        }
    }

    void run_once(size_t chunk) {
        const auto& a = (*in_)[p4_.a];
        const auto& b = (*in_)[p4_.b];
        const auto& c = (*in_)[p4_.c];
        const auto& d = (*in_)[p4_.d];

        parallel_for(n_, threads_, [&](size_t s, size_t e) {
            for (size_t block = s; block < e; block += chunk) {
                const size_t end = std::min(block + chunk, e);

                for (size_t i = block; i < end; ++i) {
                    stage2_[i] = apply(p4_.op1, a[i], b[i]); // 写入第二数组
                }
                for (size_t i = block; i < end; ++i) {
                    stage3_[i] = apply(p4_.op2, stage2_[i], c[i]); // 写入第三数组
                }
                for (size_t i = block; i < end; ++i) {
                    out_[i] = apply(p4_.op3, stage3_[i], d[i]); // 写入第四数组
                }
            }
        });
    }

    size_t tune_chunk() {
        static constexpr size_t kCandidates[] = {
            32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 4096
        };
        size_t best = 256;
        double best_ms = std::numeric_limits<double>::max();

        for (size_t c : kCandidates) {
            if (c == 0 || c > n_) continue;
            run_once(c); // warmup
            auto st = std::chrono::high_resolution_clock::now();
            run_once(c);
            auto ed = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> d = ed - st;
            if (d.count() < best_ms) {
                best_ms = d.count();
                best = c;
            }
        }
        if (best == 0) best = std::min<size_t>(256, std::max<size_t>(1, n_));
        return best;
    }

    double sample_guard() const {
        if (out_.empty()) return 0.0;
        size_t step = std::max<size_t>(1, out_.size() / 32);
        double g = 0.0;
        for (size_t i = 0; i < out_.size(); i += step) g += static_cast<double>(out_[i]);
        return g;
    }

private:
    const Program* prog_ = nullptr;
    const std::vector<std::vector<T>>* in_ = nullptr;
    size_t n_ = 0;
    int threads_ = 1;
    bool avail_ = false;
    std::string reason_;
    ChunkPipeline4Pattern p4_{};
    size_t best_chunk_ = 256;
    std::vector<T> stage2_;
    std::vector<T> stage3_;
    std::vector<T> out_;
};

template <NodeKind Op, typename T>
struct ChunkOp;
template <typename T>
struct ChunkOp<NodeKind::Add, T> { static inline T eval(T a, T b) { return a + b; } };
template <typename T>
struct ChunkOp<NodeKind::Sub, T> { static inline T eval(T a, T b) { return a - b; } };
template <typename T>
struct ChunkOp<NodeKind::Mul, T> { static inline T eval(T a, T b) { return a * b; } };
template <typename T>
struct ChunkOp<NodeKind::Div, T> { static inline T eval(T a, T b) { return a / b; } };

template <typename T>
class ChunkPipelinePeakMethod final : public IMethod<T> {
public:
    // 补强版：与既有版本并列展示，不替换
    std::string name() const override { return "chunk_pipeline_nonjit_peak"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int threads) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        threads_ = threads;
        out_.assign(n_, T{});
        stage2_.assign(n_, T{});
        stage3_.assign(n_, T{});
        avail_ = false;
        reason_.clear();

        p4_ = {};
        if (!match_chunk_pipeline4(*prog_, p4_)) {
            reason_ = "expression not supported by chunk pipeline pattern";
            return false;
        }
        if (p4_.a < 0 || p4_.b < 0 || p4_.c < 0 || p4_.d < 0 ||
            p4_.a >= static_cast<int>(in_->size()) ||
            p4_.b >= static_cast<int>(in_->size()) ||
            p4_.c >= static_cast<int>(in_->size()) ||
            p4_.d >= static_cast<int>(in_->size())) {
            reason_ = "invalid variable mapping for chunk pipeline";
            return false;
        }

        best_chunk_ = tune_chunk_robust();
        avail_ = true;
        return true;
    }

    bool available() const override { return avail_; }
    std::string unavailable_reason() const override { return reason_; }

    void run_validate(std::vector<T>& out) override {
        run_once(best_chunk_);
        out = out_;
    }

    double run_timed() override {
        run_once(best_chunk_);
        return sample_guard();
    }

private:
    template <NodeKind Op1, NodeKind Op2, NodeKind Op3>
    void run_ops(size_t chunk) {
        const T* ap = (*in_)[p4_.a].data();
        const T* bp = (*in_)[p4_.b].data();
        const T* cp = (*in_)[p4_.c].data();
        const T* dp = (*in_)[p4_.d].data();
        T* s2 = stage2_.data();
        T* s3 = stage3_.data();
        T* out = out_.data();

        parallel_for(n_, threads_, [&](size_t s, size_t e) {
            for (size_t block = s; block < e; block += chunk) {
                const size_t end = std::min(block + chunk, e);
                for (size_t i = block; i < end; ++i) {
                    s2[i] = ChunkOp<Op1, T>::eval(ap[i], bp[i]);
                }
                for (size_t i = block; i < end; ++i) {
                    s3[i] = ChunkOp<Op2, T>::eval(s2[i], cp[i]);
                }
                for (size_t i = block; i < end; ++i) {
                    out[i] = ChunkOp<Op3, T>::eval(s3[i], dp[i]);
                }
            }
        });
    }

    template <NodeKind Op1, NodeKind Op2>
    void run_stage3(NodeKind op3, size_t chunk) {
        switch (op3) {
        case NodeKind::Add: return run_ops<Op1, Op2, NodeKind::Add>(chunk);
        case NodeKind::Sub: return run_ops<Op1, Op2, NodeKind::Sub>(chunk);
        case NodeKind::Mul: return run_ops<Op1, Op2, NodeKind::Mul>(chunk);
        case NodeKind::Div: return run_ops<Op1, Op2, NodeKind::Div>(chunk);
        default: return;
        }
    }

    template <NodeKind Op1>
    void run_stage2(NodeKind op2, NodeKind op3, size_t chunk) {
        switch (op2) {
        case NodeKind::Add: return run_stage3<Op1, NodeKind::Add>(op3, chunk);
        case NodeKind::Sub: return run_stage3<Op1, NodeKind::Sub>(op3, chunk);
        case NodeKind::Mul: return run_stage3<Op1, NodeKind::Mul>(op3, chunk);
        case NodeKind::Div: return run_stage3<Op1, NodeKind::Div>(op3, chunk);
        default: return;
        }
    }

    void run_once(size_t chunk) {
        switch (p4_.op1) {
        case NodeKind::Add: return run_stage2<NodeKind::Add>(p4_.op2, p4_.op3, chunk);
        case NodeKind::Sub: return run_stage2<NodeKind::Sub>(p4_.op2, p4_.op3, chunk);
        case NodeKind::Mul: return run_stage2<NodeKind::Mul>(p4_.op2, p4_.op3, chunk);
        case NodeKind::Div: return run_stage2<NodeKind::Div>(p4_.op2, p4_.op3, chunk);
        default: return;
        }
    }

    size_t tune_chunk_robust() {
        static constexpr size_t kCandidates[] = {
            32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 4096, 6144
        };
        size_t best = std::min<size_t>(256, std::max<size_t>(1, n_));
        double best_ms = std::numeric_limits<double>::max();

        std::vector<size_t> candidates;
        for (size_t c : kCandidates) {
            if (c > 0 && c <= n_) candidates.push_back(c);
        }
        if (candidates.empty()) return best;

        for (size_t c : candidates) {
            run_once(c); // warmup
            std::array<double, 3> samples{};
            for (double& s : samples) {
                auto st = std::chrono::high_resolution_clock::now();
                run_once(c);
                auto ed = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> d = ed - st;
                s = d.count();
            }
            std::sort(samples.begin(), samples.end());
            const double median_ms = samples[1];
            if (median_ms < best_ms) {
                best_ms = median_ms;
                best = c;
            }
        }
        return best;
    }

    double sample_guard() const {
        if (out_.empty()) return 0.0;
        size_t step = std::max<size_t>(1, out_.size() / 32);
        double g = 0.0;
        for (size_t i = 0; i < out_.size(); i += step) g += static_cast<double>(out_[i]);
        return g;
    }

private:
    const Program* prog_ = nullptr;
    const std::vector<std::vector<T>>* in_ = nullptr;
    size_t n_ = 0;
    int threads_ = 1;
    bool avail_ = false;
    std::string reason_;
    ChunkPipeline4Pattern p4_{};
    size_t best_chunk_ = 256;
    std::vector<T> stage2_;
    std::vector<T> stage3_;
    std::vector<T> out_;
};

} // namespace exprbench


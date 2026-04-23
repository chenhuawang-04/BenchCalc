#pragma once

#include "method_interface.hpp"

namespace exprbench {

template <NodeKind Op, typename T>
struct PlainLoopOp;
template <typename T>
struct PlainLoopOp<NodeKind::Add, T> { static inline T eval(T a, T b) { return a + b; } };
template <typename T>
struct PlainLoopOp<NodeKind::Sub, T> { static inline T eval(T a, T b) { return a - b; } };
template <typename T>
struct PlainLoopOp<NodeKind::Mul, T> { static inline T eval(T a, T b) { return a * b; } };
template <typename T>
struct PlainLoopOp<NodeKind::Div, T> { static inline T eval(T a, T b) { return a / b; } };

template <typename T>
class PlainLoop4Method final : public IMethod<T> {
public:
    // 对照组：最普通的 4 数组单循环硬编码，不分块、不调优
    std::string name() const override { return "hardcoded_plain_loop4"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int /*threads*/) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        out_.assign(n_, T{});
        avail_ = false;
        reason_.clear();
        p4_ = {};

        if (!match_chunk_pipeline4(*prog_, p4_)) {
            reason_ = "expression not supported by plain loop4 baseline";
            return false;
        }
        if (p4_.a < 0 || p4_.b < 0 || p4_.c < 0 || p4_.d < 0 ||
            p4_.a >= static_cast<int>(in_->size()) ||
            p4_.b >= static_cast<int>(in_->size()) ||
            p4_.c >= static_cast<int>(in_->size()) ||
            p4_.d >= static_cast<int>(in_->size())) {
            reason_ = "invalid variable mapping for plain loop4 baseline";
            return false;
        }
        avail_ = true;
        return true;
    }

    bool available() const override { return avail_; }
    std::string unavailable_reason() const override { return reason_; }

    void run_validate(std::vector<T>& out) override {
        run_once();
        out = out_;
    }

    double run_timed() override {
        run_once();
        return sample_guard();
    }

private:
    template <NodeKind Op1, NodeKind Op2, NodeKind Op3>
    void run_ops() {
        const auto& a = (*in_)[p4_.a];
        const auto& b = (*in_)[p4_.b];
        const auto& c = (*in_)[p4_.c];
        const auto& d = (*in_)[p4_.d];

        // 最普通单循环：for(i) out[i] = ((a op b) op c) op d
        for (size_t i = 0; i < n_; ++i) {
            const T t1 = PlainLoopOp<Op1, T>::eval(a[i], b[i]);
            const T t2 = PlainLoopOp<Op2, T>::eval(t1, c[i]);
            out_[i] = PlainLoopOp<Op3, T>::eval(t2, d[i]);
        }
    }

    template <NodeKind Op1, NodeKind Op2>
    void run_stage3(NodeKind op3) {
        switch (op3) {
        case NodeKind::Add: return run_ops<Op1, Op2, NodeKind::Add>();
        case NodeKind::Sub: return run_ops<Op1, Op2, NodeKind::Sub>();
        case NodeKind::Mul: return run_ops<Op1, Op2, NodeKind::Mul>();
        case NodeKind::Div: return run_ops<Op1, Op2, NodeKind::Div>();
        default: return;
        }
    }

    template <NodeKind Op1>
    void run_stage2(NodeKind op2, NodeKind op3) {
        switch (op2) {
        case NodeKind::Add: return run_stage3<Op1, NodeKind::Add>(op3);
        case NodeKind::Sub: return run_stage3<Op1, NodeKind::Sub>(op3);
        case NodeKind::Mul: return run_stage3<Op1, NodeKind::Mul>(op3);
        case NodeKind::Div: return run_stage3<Op1, NodeKind::Div>(op3);
        default: return;
        }
    }

    void run_once() {
        switch (p4_.op1) {
        case NodeKind::Add: return run_stage2<NodeKind::Add>(p4_.op2, p4_.op3);
        case NodeKind::Sub: return run_stage2<NodeKind::Sub>(p4_.op2, p4_.op3);
        case NodeKind::Mul: return run_stage2<NodeKind::Mul>(p4_.op2, p4_.op3);
        case NodeKind::Div: return run_stage2<NodeKind::Div>(p4_.op2, p4_.op3);
        default: return;
        }
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
    bool avail_ = false;
    std::string reason_;
    ChunkPipeline4Pattern p4_{};
    std::vector<T> out_;
};

template <typename T>
class PlainInplaceAmsMethod final : public IMethod<T> {
public:
    // 你要求的最普通原地硬编码：a[i] = (a[i] + b[i]) * c[i] - d[i]
    std::string name() const override { return "hardcoded_plain_inplace_ams"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int /*threads*/) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        avail_ = false;
        reason_.clear();
        p4_ = {};
        out_.assign(n_, T{});
        a_base_.assign(n_, T{});
        a_work_.assign(n_, T{});

        if (!match_chunk_pipeline4(*prog_, p4_)) {
            reason_ = "inplace ams baseline only supports chunk pipeline pattern";
            return false;
        }
        if (!(p4_.op1 == NodeKind::Add && p4_.op2 == NodeKind::Mul && p4_.op3 == NodeKind::Sub)) {
            reason_ = "inplace ams baseline only supports ((a+b)*c)-d";
            return false;
        }
        if (p4_.a < 0 || p4_.b < 0 || p4_.c < 0 || p4_.d < 0 ||
            p4_.a >= static_cast<int>(in_->size()) ||
            p4_.b >= static_cast<int>(in_->size()) ||
            p4_.c >= static_cast<int>(in_->size()) ||
            p4_.d >= static_cast<int>(in_->size())) {
            reason_ = "invalid variable mapping for inplace ams baseline";
            return false;
        }

        a_base_ = (*in_)[p4_.a];
        avail_ = true;
        return true;
    }

    bool available() const override { return avail_; }
    std::string unavailable_reason() const override { return reason_; }

    void run_validate(std::vector<T>& out) override {
        run_once();
        out = out_;
    }

    double run_timed() override {
        run_once();
        return sample_guard();
    }

private:
    void run_once() {
        // 为了科学可复现，每次运行前恢复初值，再执行原地更新循环。
        a_work_ = a_base_;
        const auto& b = (*in_)[p4_.b];
        const auto& c = (*in_)[p4_.c];
        const auto& d = (*in_)[p4_.d];

        for (size_t i = 0; i < n_; ++i) {
            a_work_[i] = (a_work_[i] + b[i]) * c[i] - d[i];
        }
        out_ = a_work_;
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
    bool avail_ = false;
    std::string reason_;
    ChunkPipeline4Pattern p4_{};
    std::vector<T> a_base_;
    std::vector<T> a_work_;
    std::vector<T> out_;
};

} // namespace exprbench

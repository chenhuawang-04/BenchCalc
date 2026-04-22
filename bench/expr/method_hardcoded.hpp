#pragma once

#include "method_interface.hpp"

namespace exprbench {

template <typename T>
class HardcodedInlineMethod final : public IMethod<T> {
public:
    std::string name() const override { return "hardcoded_inline"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int threads) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        threads_ = threads;
        out_.assign(n_, T{});
        avail_ = false;
        reason_.clear();
        c6_ = {};
        p4_ = {};
        r8_ = {};
        cp4_ = {};

        if (match_classic6(*prog_, c6_)) {
            mode_ = Mode::Classic6;
            avail_ = true;
        } else if (match_poly4(*prog_, p4_)) {
            mode_ = Mode::Poly4;
            avail_ = true;
        } else if (match_reduction8(*prog_, r8_)) {
            mode_ = Mode::Reduction8;
            avail_ = true;
        } else if (match_chunk_pipeline4(*prog_, cp4_)) {
            mode_ = Mode::ChunkPipeline4;
            avail_ = true;
        } else {
            mode_ = Mode::Unsupported;
            reason_ = "expression not supported by hardcoded baseline";
        }
        return avail_;
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
    enum class Mode { Unsupported, Classic6, Poly4, Reduction8, ChunkPipeline4 };

    static inline T apply(NodeKind op, T lhs, T rhs) {
        switch (op) {
        case NodeKind::Add: return lhs + rhs;
        case NodeKind::Sub: return lhs - rhs;
        case NodeKind::Mul: return lhs * rhs;
        case NodeKind::Div: return lhs / rhs;
        default: return lhs;
        }
    }

    void run_once() {
        switch (mode_) {
        case Mode::Classic6: run_classic6(); break;
        case Mode::Poly4: run_poly4(); break;
        case Mode::Reduction8: run_reduction8(); break;
        case Mode::ChunkPipeline4: run_chunk_pipeline4(); break;
        default: break;
        }
    }
    void run_classic6() {
        const auto& a = (*in_)[c6_.a];
        const auto& b = (*in_)[c6_.b];
        const auto& c = (*in_)[c6_.c];
        const auto& d = (*in_)[c6_.d];
        const auto& e = (*in_)[c6_.e];
        const auto& f = (*in_)[c6_.f];
        parallel_for(n_, threads_, [&](size_t s, size_t eidx) {
            for (size_t i = s; i < eidx; ++i) out_[i] = a[i] + b[i] - c[i] * d[i] * e[i] + f[i];
        });
    }
    void run_poly4() {
        const auto& a = (*in_)[p4_.a];
        const auto& b = (*in_)[p4_.b];
        const auto& c = (*in_)[p4_.c];
        const auto& d = (*in_)[p4_.d];
        parallel_for(n_, threads_, [&](size_t s, size_t eidx) {
            for (size_t i = s; i < eidx; ++i) out_[i] = a[i] + b[i] * c[i] - d[i];
        });
    }
    void run_reduction8() {
        const auto& a = (*in_)[r8_.v[0]];
        const auto& b = (*in_)[r8_.v[1]];
        const auto& c = (*in_)[r8_.v[2]];
        const auto& d = (*in_)[r8_.v[3]];
        const auto& e = (*in_)[r8_.v[4]];
        const auto& f = (*in_)[r8_.v[5]];
        const auto& g = (*in_)[r8_.v[6]];
        const auto& h = (*in_)[r8_.v[7]];
        parallel_for(n_, threads_, [&](size_t s, size_t eidx) {
            for (size_t i = s; i < eidx; ++i) out_[i] = a[i] + b[i] + c[i] + d[i] + e[i] + f[i] + g[i] + h[i];
        });
    }

    void run_chunk_pipeline4() {
        const auto& a = (*in_)[cp4_.a];
        const auto& b = (*in_)[cp4_.b];
        const auto& c = (*in_)[cp4_.c];
        const auto& d = (*in_)[cp4_.d];
        parallel_for(n_, threads_, [&](size_t s, size_t eidx) {
            for (size_t i = s; i < eidx; ++i) {
                const T t1 = apply(cp4_.op1, a[i], b[i]);
                const T t2 = apply(cp4_.op2, t1, c[i]);
                out_[i] = apply(cp4_.op3, t2, d[i]);
            }
        });
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
    Mode mode_ = Mode::Unsupported;
    Classic6Pattern c6_;
    Poly4Pattern p4_;
    Reduction8Pattern r8_;
    ChunkPipeline4Pattern cp4_;
    std::vector<T> out_;
};

} // namespace exprbench

#pragma once

#include "method_interface.hpp"

namespace exprbench {

template <typename T>
class GraphFusedMethod final : public IMethod<T> {
public:
    std::string name() const override { return "graph_fused_kernellib"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int threads) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        threads_ = threads;
        out_.assign(n_, T{});
        c6_ = {};
        p4_ = {};
        r8_ = {};
        mode_ = Mode::Generic;
        eval_nodes_.clear();
        node_to_slot_.assign(prog_->nodes.size(), -1);
        temp_buffers_.clear();

        if (match_classic6(*prog_, c6_)) {
            mode_ = Mode::Classic6;
        } else if (match_poly4(*prog_, p4_)) {
            mode_ = Mode::Poly4;
        } else if (match_reduction8(*prog_, r8_)) {
            mode_ = Mode::Reduction8;
        } else {
            for (size_t nid = 0; nid < prog_->nodes.size(); ++nid) {
                const NodeKind k = prog_->nodes[nid].kind;
                if (k == NodeKind::Var || k == NodeKind::Const) continue;
                node_to_slot_[nid] = static_cast<int>(eval_nodes_.size());
                eval_nodes_.push_back(static_cast<int>(nid));
            }
            temp_buffers_.resize(eval_nodes_.size(), std::vector<T>(n_, T{}));
        }

        avail_ = true;
        reason_.clear();
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
    enum class Mode { Generic, Classic6, Poly4, Reduction8 };

    void run_once() {
        switch (mode_) {
        case Mode::Classic6: run_classic6(); return;
        case Mode::Poly4: run_poly4(); return;
        case Mode::Reduction8: run_reduction8(); return;
        case Mode::Generic: run_generic(); return;
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

    inline T read_operand(int node_id, size_t i) const {
        const Node& c = prog_->nodes[node_id];
        if (c.kind == NodeKind::Var) return (*in_)[c.var_index][i];
        if (c.kind == NodeKind::Const) return static_cast<T>(c.const_value);
        const int slot = node_to_slot_[node_id];
        return temp_buffers_[static_cast<size_t>(slot)][i];
    }

    void run_generic() {
        for (size_t sid = 0; sid < eval_nodes_.size(); ++sid) {
            const int nid = eval_nodes_[sid];
            const Node& node = prog_->nodes[nid];
            auto& dst = temp_buffers_[sid];
            parallel_for(n_, threads_, [&](size_t s, size_t eidx) {
                for (size_t i = s; i < eidx; ++i) {
                    switch (node.kind) {
                    case NodeKind::Neg:
                        dst[i] = -read_operand(node.lhs, i);
                        break;
                    case NodeKind::Add:
                        dst[i] = read_operand(node.lhs, i) + read_operand(node.rhs, i);
                        break;
                    case NodeKind::Sub:
                        dst[i] = read_operand(node.lhs, i) - read_operand(node.rhs, i);
                        break;
                    case NodeKind::Mul:
                        dst[i] = read_operand(node.lhs, i) * read_operand(node.rhs, i);
                        break;
                    case NodeKind::Div:
                        dst[i] = read_operand(node.lhs, i) / read_operand(node.rhs, i);
                        break;
                    default:
                        break;
                    }
                }
            });
        }

        const Node& r = prog_->nodes[prog_->root];
        if (r.kind == NodeKind::Var) {
            out_ = (*in_)[r.var_index];
        } else if (r.kind == NodeKind::Const) {
            std::fill(out_.begin(), out_.end(), static_cast<T>(r.const_value));
        } else {
            const int slot = node_to_slot_[prog_->root];
            out_ = temp_buffers_[static_cast<size_t>(slot)];
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
    int threads_ = 1;
    bool avail_ = false;
    std::string reason_;
    Mode mode_ = Mode::Generic;
    Classic6Pattern c6_;
    Poly4Pattern p4_;
    Reduction8Pattern r8_;
    std::vector<int> eval_nodes_;
    std::vector<int> node_to_slot_;
    std::vector<std::vector<T>> temp_buffers_;
    std::vector<T> out_;
};

} // namespace exprbench


#pragma once

#include "method_interface.hpp"

namespace exprbench {

template <typename T>
class VmMethod final : public IMethod<T> {
public:
    std::string name() const override { return "vm_register"; }

    bool prepare(const Program& prog,
                 const std::vector<std::vector<T>>& inputs,
                 size_t n,
                 int threads) override {
        prog_ = &prog;
        in_ = &inputs;
        n_ = n;
        threads_ = threads;
        out_.assign(n_, T{});
        instructions_.clear();
        reg_count_ = 0;
        root_ = compile_node(prog_->root);
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
    enum class Op { Add, Sub, Mul, Div, Neg };
    enum class OType { Reg, Var, Const };
    struct Operand {
        OType type = OType::Const;
        int index = -1;
        double c = 0.0;
    };
    struct Inst {
        Op op = Op::Add;
        int dst = -1;
        Operand a;
        Operand b;
        bool unary = false;
    };

    Operand compile_node(int id) {
        const Node& n = prog_->nodes[id];
        if (n.kind == NodeKind::Var) return Operand{OType::Var, n.var_index, 0.0};
        if (n.kind == NodeKind::Const) return Operand{OType::Const, -1, n.const_value};
        if (n.kind == NodeKind::Neg) {
            Operand x = compile_node(n.lhs);
            Inst inst;
            inst.op = Op::Neg;
            inst.dst = reg_count_++;
            inst.a = x;
            inst.unary = true;
            instructions_.push_back(inst);
            return Operand{OType::Reg, inst.dst, 0.0};
        }
        Operand a = compile_node(n.lhs);
        Operand b = compile_node(n.rhs);
        Inst inst;
        inst.dst = reg_count_++;
        inst.a = a;
        inst.b = b;
        switch (n.kind) {
        case NodeKind::Add: inst.op = Op::Add; break;
        case NodeKind::Sub: inst.op = Op::Sub; break;
        case NodeKind::Mul: inst.op = Op::Mul; break;
        case NodeKind::Div: inst.op = Op::Div; break;
        default: break;
        }
        instructions_.push_back(inst);
        return Operand{OType::Reg, inst.dst, 0.0};
    }

    void run_once() {
        auto read = [&](const Operand& op, const std::vector<T>& regs, size_t i) -> T {
            switch (op.type) {
            case OType::Reg: return regs[op.index];
            case OType::Var: return (*in_)[op.index][i];
            case OType::Const: return static_cast<T>(op.c);
            default: return static_cast<T>(0);
            }
        };
        parallel_for(n_, threads_, [&](size_t s, size_t eidx) {
            std::vector<T> regs(static_cast<size_t>(reg_count_), T{});
            for (size_t i = s; i < eidx; ++i) {
                for (const auto& inst : instructions_) {
                    const T a = read(inst.a, regs, i);
                    T v = 0;
                    if (inst.unary) {
                        v = -a;
                    } else {
                        const T b = read(inst.b, regs, i);
                        switch (inst.op) {
                        case Op::Add: v = a + b; break;
                        case Op::Sub: v = a - b; break;
                        case Op::Mul: v = a * b; break;
                        case Op::Div: v = a / b; break;
                        default: break;
                        }
                    }
                    regs[inst.dst] = v;
                }
                switch (root_.type) {
                case OType::Reg: out_[i] = regs[root_.index]; break;
                case OType::Var: out_[i] = (*in_)[root_.index][i]; break;
                case OType::Const: out_[i] = static_cast<T>(root_.c); break;
                }
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
    std::vector<Inst> instructions_;
    int reg_count_ = 0;
    Operand root_;
    std::vector<T> out_;
};

} // namespace exprbench

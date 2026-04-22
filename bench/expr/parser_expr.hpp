#pragma once

#include "parser_json.hpp"

namespace exprbench {

enum class TokKind { Identifier, Number, Plus, Minus, Star, Slash, LParen, RParen, End };
struct Token {
    TokKind kind = TokKind::End;
    std::string text;
    double number = 0.0;
};

class ExprParser {
public:
    explicit ExprParser(const ExprSpec& spec)
        : spec_(spec), text_(spec.expr), pos_(0), cur_(next_token()) {
        for (size_t i = 0; i < spec_.variables.size(); ++i) {
            var_index_[spec_.variables[i]] = static_cast<int>(i);
        }
    }

    Program parse_program() {
        Program p;
        p.variables = spec_.variables;
        p.expr_string = spec_.expr;
        nodes_.clear();
        const int root = parse_expression();
        if (cur_.kind != TokKind::End) {
            throw std::runtime_error("parse error: trailing token");
        }
        p.nodes = nodes_;
        p.root = root;
        return p;
    }

private:
    int parse_expression() {
        int lhs = parse_term();
        while (cur_.kind == TokKind::Plus || cur_.kind == TokKind::Minus) {
            TokKind op = cur_.kind;
            cur_ = next_token();
            int rhs = parse_term();
            lhs = make_binary(op == TokKind::Plus ? NodeKind::Add : NodeKind::Sub, lhs, rhs);
        }
        return lhs;
    }

    int parse_term() {
        int lhs = parse_factor();
        while (cur_.kind == TokKind::Star || cur_.kind == TokKind::Slash) {
            TokKind op = cur_.kind;
            cur_ = next_token();
            int rhs = parse_factor();
            lhs = make_binary(op == TokKind::Star ? NodeKind::Mul : NodeKind::Div, lhs, rhs);
        }
        return lhs;
    }

    int parse_factor() {
        if (cur_.kind == TokKind::Minus) {
            cur_ = next_token();
            int x = parse_factor();
            return make_unary(NodeKind::Neg, x);
        }
        if (cur_.kind == TokKind::LParen) {
            cur_ = next_token();
            int x = parse_expression();
            if (cur_.kind != TokKind::RParen) throw std::runtime_error("parse error: expected ')'");
            cur_ = next_token();
            return x;
        }
        if (cur_.kind == TokKind::Number) {
            int id = add_const(cur_.number);
            cur_ = next_token();
            return id;
        }
        if (cur_.kind == TokKind::Identifier) {
            auto it = var_index_.find(cur_.text);
            if (it == var_index_.end()) throw std::runtime_error("unknown variable: " + cur_.text);
            int id = add_var(it->second);
            cur_ = next_token();
            return id;
        }
        throw std::runtime_error("parse error near: " + cur_.text);
    }

    int add_const(double v) {
        Node n;
        n.kind = NodeKind::Const;
        n.const_value = v;
        nodes_.push_back(n);
        return static_cast<int>(nodes_.size()) - 1;
    }
    int add_var(int idx) {
        Node n;
        n.kind = NodeKind::Var;
        n.var_index = idx;
        nodes_.push_back(n);
        return static_cast<int>(nodes_.size()) - 1;
    }
    int make_unary(NodeKind kind, int x) {
        if (nodes_[x].kind == NodeKind::Const && kind == NodeKind::Neg) {
            return add_const(-nodes_[x].const_value);
        }
        Node n;
        n.kind = kind;
        n.lhs = x;
        nodes_.push_back(n);
        return static_cast<int>(nodes_.size()) - 1;
    }
    int make_binary(NodeKind kind, int a, int b) {
        const Node& na = nodes_[a];
        const Node& nb = nodes_[b];
        if (na.kind == NodeKind::Const && nb.kind == NodeKind::Const) {
            switch (kind) {
            case NodeKind::Add: return add_const(na.const_value + nb.const_value);
            case NodeKind::Sub: return add_const(na.const_value - nb.const_value);
            case NodeKind::Mul: return add_const(na.const_value * nb.const_value);
            case NodeKind::Div: return add_const(na.const_value / nb.const_value);
            default: break;
            }
        }
        Node n;
        n.kind = kind;
        n.lhs = a;
        n.rhs = b;
        nodes_.push_back(n);
        return static_cast<int>(nodes_.size()) - 1;
    }

    Token next_token() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (pos_ >= text_.size()) return Token{TokKind::End, "", 0.0};

        const char c = text_[pos_];
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const size_t start = pos_++;
            while (pos_ < text_.size()) {
                const char ch = text_[pos_];
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') break;
                ++pos_;
            }
            return Token{TokKind::Identifier, text_.substr(start, pos_ - start), 0.0};
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            const size_t start = pos_++;
            while (pos_ < text_.size()) {
                const char ch = text_[pos_];
                if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '.' || ch == 'e' || ch == 'E' ||
                      ch == '+' || ch == '-')) {
                    break;
                }
                if ((ch == '+' || ch == '-') &&
                    !(text_[pos_ - 1] == 'e' || text_[pos_ - 1] == 'E')) {
                    break;
                }
                ++pos_;
            }
            const std::string s = text_.substr(start, pos_ - start);
            return Token{TokKind::Number, s, std::stod(s)};
        }
        ++pos_;
        switch (c) {
        case '+': return Token{TokKind::Plus, "+", 0.0};
        case '-': return Token{TokKind::Minus, "-", 0.0};
        case '*': return Token{TokKind::Star, "*", 0.0};
        case '/': return Token{TokKind::Slash, "/", 0.0};
        case '(': return Token{TokKind::LParen, "(", 0.0};
        case ')': return Token{TokKind::RParen, ")", 0.0};
        default: throw std::runtime_error(std::string("invalid char in expr: ") + c);
        }
    }

private:
    const ExprSpec& spec_;
    std::string text_;
    size_t pos_ = 0;
    Token cur_;
    std::unordered_map<std::string, int> var_index_;
    std::vector<Node> nodes_;
};

inline Program parse_program_from_json(const std::string& json) {
    ExprSpec spec = parse_expression_json(json);
    ExprParser p(spec);
    return p.parse_program();
}

} // namespace exprbench


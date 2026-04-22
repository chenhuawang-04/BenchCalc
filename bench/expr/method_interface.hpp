#pragma once

#include "core_ast_parser.hpp"

namespace exprbench {

template <typename T>
class IMethod {
public:
    virtual ~IMethod() = default;
    virtual std::string name() const = 0;
    virtual bool prepare(const Program& prog,
                         const std::vector<std::vector<T>>& inputs,
                         size_t n,
                         int threads) = 0;
    virtual bool available() const = 0;
    virtual std::string unavailable_reason() const = 0;
    virtual void run_validate(std::vector<T>& out) = 0;
    virtual double run_timed() = 0;
};

} // namespace exprbench

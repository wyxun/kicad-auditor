#pragma once

#include <string>
#include <vector>
#include <string_view>
#include "types.hpp"
#include "sexpr.hpp"
#include "report.hpp"

namespace auditor {

/**
 * @brief 原理图审计规则抽象基类
 */
class SchRule {
public:
    virtual ~SchRule() = default;
    virtual std::string get_id() const = 0;
    virtual std::string get_description() const = 0;
    virtual void analyze(const SExpr& sch, Report& report) = 0;
};

/**
 * @brief PCB 审计规则抽象基类
 */
class PcbRule {
public:
    virtual ~PcbRule() = default;
    virtual std::string get_id() const = 0;
    virtual std::string get_description() const = 0;
    virtual void analyze(const SExpr& pcb, Report& report) = 0;
};

} // namespace auditor

#pragma once

#include <vector>
#include <memory>
#include "rule.hpp"

namespace auditor {

/**
 * @brief 原理图审计规则注册与管理器
 */
class SchRuleRegistry {
private:
    std::vector<std::unique_ptr<SchRule>> rules_;

public:
    SchRuleRegistry() = default;

    // 禁止拷贝
    SchRuleRegistry(const SchRuleRegistry&) = delete;
    SchRuleRegistry& operator=(const SchRuleRegistry&) = delete;

    // 支持移动
    SchRuleRegistry(SchRuleRegistry&&) noexcept = default;
    SchRuleRegistry& operator=(SchRuleRegistry&&) noexcept = default;

    /**
     * @brief 注册一条新原理图规则
     */
    void register_rule(std::unique_ptr<SchRule> rule) {
        if (rule) {
            rules_.push_back(std::move(rule));
        }
    }

    /**
     * @brief 一键执行所有已注册的原理图规则并汇总报告
     */
    Report run_all(const SExpr& sch) {
        Report report;
        for (const auto& rule : rules_) {
            if (rule) {
                rule->analyze(sch, report);
            }
        }
        return report;
    }

    /**
     * @brief 获取所有已注册的规则
     */
    const std::vector<std::unique_ptr<SchRule>>& get_rules() const {
        return rules_;
    }

    /**
     * @brief 清空已注册的所有规则
     */
    void clear() {
        rules_.clear();
    }
};

/**
 * @brief PCB 审计规则注册与管理器
 */
class PcbRuleRegistry {
private:
    std::vector<std::unique_ptr<PcbRule>> rules_;

public:
    PcbRuleRegistry() = default;

    // 禁止拷贝
    PcbRuleRegistry(const PcbRuleRegistry&) = delete;
    PcbRuleRegistry& operator=(const PcbRuleRegistry&) = delete;

    // 支持移动
    PcbRuleRegistry(PcbRuleRegistry&&) noexcept = default;
    PcbRuleRegistry& operator=(PcbRuleRegistry&&) noexcept = default;

    /**
     * @brief 注册一条新 PCB 规则
     */
    void register_rule(std::unique_ptr<PcbRule> rule) {
        if (rule) {
            rules_.push_back(std::move(rule));
        }
    }

    /**
     * @brief 一键执行所有已注册的 PCB 规则并汇总报告
     */
    Report run_all(const SExpr& pcb) {
        Report report;
        for (const auto& rule : rules_) {
            if (rule) {
                rule->analyze(pcb, report);
            }
        }
        return report;
    }

    /**
     * @brief 获取所有已注册的规则
     */
    const std::vector<std::unique_ptr<PcbRule>>& get_rules() const {
        return rules_;
    }

    /**
     * @brief 清空已注册的所有规则
     */
    void clear() {
        rules_.clear();
    }
};

} // namespace auditor

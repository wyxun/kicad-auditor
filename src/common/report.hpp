#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace auditor {

/**
 * @brief 诊断违规条目
 */
struct Violation {
    std::string rule_id;       // 违反的规则ID (e.g. "SCH_ISO_01", "EMI_CLEARANCE")
    std::string severity;      // 严重程度: "ERROR", "WARNING", "INFO"
    std::string component_ref; // 组件引用/定位 (e.g. "Q1", "Net: Vadj")
    std::string description;   // 详细缺陷描述信息
    
    // 向后兼容旧代码的字段
    std::string location;      // 定位信息 (在构造时与 component_ref 同步)
    std::string message;       // 缺陷描述 (在构造时与 description 同步)

    Violation(std::string_view r_id, std::string_view sev, std::string_view desc, std::string_view ref)
        : rule_id(r_id), severity(sev), component_ref(ref), description(desc), location(ref), message(desc) {}
};

/**
 * @brief 诊断报告聚合与导出类
 */
class Report {
public:
    std::vector<Violation> violations;

    /**
     * @brief 增加一条违规诊断
     */
    void add_violation(std::string_view rule_id, std::string_view severity, std::string_view message, std::string_view location = "") {
        violations.emplace_back(rule_id, severity, message, location);
    }

    /**
     * @brief 检查报告中是否包含 ERROR 级别缺陷
     */
    bool has_errors() const {
        for (const auto& v : violations) {
            if (v.severity == "ERROR") return true;
        }
        return false;
    }

    /**
     * @brief 导出美观的 GitHub-flavored Markdown 格式硬件审计分析报告
     * @param filepath 输出报告的文件绝对/相对路径
     * @param project_name 硬件工程名字或路径
     */
    void export_markdown(const std::string& filepath, const std::string& project_name) const;
};

} // namespace auditor

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../common/types.hpp"
#include "../common/sexpr.hpp"
#include "../common/rule.hpp"
#include "../common/registry.hpp"

namespace auditor {

/**
 * @brief 器件引脚拓扑信息
 */
struct PinInfo {
    std::string name;     // 引脚编号或名称，如 "1", "2", "FB", "EN"
    Point rel_pos;        // 相对器件中心的坐标 (dx, dy)
    Point abs_pos;        // 绝对坐标 (x, y)
    std::string pt_id;    // 唯一物理坐标点标识，格式如 "x.xxx,y.yyy"
};

/**
 * @brief 原理图器件拓扑信息
 */
struct ComponentInfo {
    std::string ref;      // 元器件位号，如 "U1", "R1", "C1"
    std::string value;    // 型号/值，如 "PC817", "10k", "LM2596"
    Point pos;            // 器件绝对物理位置 (x, y)
    std::vector<PinInfo> pins; // 引脚列表

    const PinInfo* find_pin(const std::string& pin_name) const {
        for (const auto& pin : pins) {
            if (pin.name == pin_name) return &pin;
        }
        return nullptr;
    }
};

class SchAnalyzer;

/**
 * @brief 线程局部上下文，用于在不破坏 SchRule 签名的情况下向规则注入 SchAnalyzer
 */
class SchAnalyzerContext {
private:
    static inline const SchAnalyzer* current_analyzer_ = nullptr;
public:
    static void set(const SchAnalyzer* analyzer) { current_analyzer_ = analyzer; }
    static const SchAnalyzer* get() { return current_analyzer_; }
};

/**
 * @brief 原理图分析器主类，负责 SExpr 解析、拓扑图构建与规则调度
 */
class SchAnalyzer {
private:
    std::vector<ComponentInfo> components_;
    
    // 物理图邻接表：点ID ("x.xxx,y.yyy") -> 连接的其他点ID列表
    std::unordered_map<std::string, std::vector<std::string>> adj_;
    
    // 点ID ("x.xxx,y.yyy") -> 网络标签名称 (如 "GND", "Vadj", "Vout")
    std::unordered_map<std::string, std::string> point_to_label_;

    // 缓存节点到最终电气 Net 名字的映射（跑完连通性后生成）
    mutable std::unordered_map<std::string, std::string> resolved_nets_;

public:
    SchAnalyzer() = default;

    /**
     * @brief 加载并解析原理图 SExpr，提取拓扑结构
     * @param sch_root 原理图根 SExpr 节点
     * @return 是否成功构建拓扑
     */
    bool load_schematic(const SExpr& sch_root);

    /**
     * @brief 一键调度已注册的原理图诊断规则并生成报告
     */
    Report analyze(SchRuleRegistry& registry);

    // 拓扑图查询接口
    const std::vector<ComponentInfo>& get_components() const { return components_; }
    const std::unordered_map<std::string, std::vector<std::string>>& get_adj() const { return adj_; }
    const std::unordered_map<std::string, std::string>& get_point_to_label() const { return point_to_label_; }

    /**
     * @brief 获取某物理坐标点所在的电气网络名称 (通过图连通性递归查找关联的 Label，若无则返回物理坐标 ID)
     */
    std::string get_net_name(const std::string& pt_id) const;

    /**
     * @brief 格式化坐标为唯一字符串键值
     */
    static std::string format_point(const Point& pt);

private:
    void clear();
    void build_topology(const SExpr& sch_root);
    void extract_symbols(const SExpr& sch_root);
    void extract_connections(const SExpr& sch_root);
};

// 诊断规则工厂函数
std::unique_ptr<SchRule> create_isolation_rule();
std::unique_ptr<SchRule> create_fb_resistor_rule();

} // namespace auditor

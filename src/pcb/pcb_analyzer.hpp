#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "../common/types.hpp"
#include "../common/sexpr.hpp"
#include "../common/registry.hpp"
#include "../common/rule.hpp"

namespace auditor {

/**
 * @brief PCB 过孔结构体 (Via)
 */
struct PcbVia {
    Point pos;                      // 过孔中心位置
    double size = 0.0;              // 过孔外径尺寸
    double drill = 0.0;             // 过孔内钻孔尺寸
    std::vector<std::string> layers;// 跨越的层
    int net = -1;                   // 所属网络编号

    /**
     * @brief 计算点到过孔边界的最短距离
     * 过孔为圆形，距离为到中心点距离减去半径 (size / 2.0)
     */
    double distance_to_point(const Point& p) const {
        double radius = size / 2.0;
        double dist = pos.distance_to(p);
        return std::max(0.0, dist - radius);
    }
};

/**
 * @brief KiCad PCB 主分析器类
 */
class PcbAnalyzer {
private:
    std::string pcb_filepath_;
    std::unique_ptr<SExpr> pcb_root_;

    // 提取出来的物理实体集合
    std::vector<PcbSegment> segments_;
    std::vector<PcbPad> pads_;
    std::vector<PcbVia> vias_;
    std::vector<PcbZone> zones_;

    // 网络映射: Net Number -> Net Name
    std::map<int, std::string> net_id_to_name_;

    // 辅助解析方法
    void parse_nets(const SExpr& pcb);
    void parse_segments(const SExpr& pcb);
    void parse_vias(const SExpr& pcb);
    void parse_footprints_and_pads(const SExpr& pcb);
    void parse_zones(const SExpr& pcb);

public:
    PcbAnalyzer() = default;

    /**
     * @brief 执行 kicad-cli 自动填充铺铜 (fill-zones)
     * @param filepath PCB文件路径
     * @return 是否执行成功
     */
    bool fill_pcb_zones(const std::string& filepath);

    /**
     * @brief 加载 PCB 文件，进行铺铜刷新与 S-Expression 解析
     * @param filepath PCB文件路径
     * @return 是否加载并解析成功
     */
    bool load_pcb(const std::string& filepath);

    /**
     * @brief 从内存中的 Mock PCB SExpr 节点直接加载，用于单元测试
     * @param pcb_root PCB S-Expr 根节点
     */
    void load_mock_pcb(const SExpr& pcb_root);

    /**
     * @brief 一键执行 PCB 规则审计
     * @param registry 已注册 PCB 规则的管理器
     * @return 包含所有违规项的报告
     */
    Report analyze(PcbRuleRegistry& registry);

    // Getters 用于规则与测试访问
    const std::vector<PcbSegment>& get_segments() const { return segments_; }
    const std::vector<PcbPad>& get_pads() const { return pads_; }
    const std::vector<PcbVia>& get_vias() const { return vias_; }
    const std::vector<PcbZone>& get_zones() const { return zones_; }
    std::string get_net_name(int net_id) const {
        auto it = net_id_to_name_.find(net_id);
        return it != net_id_to_name_.end() ? it->second : "";
    }
};

// PCB 诊断规则工厂函数
std::unique_ptr<PcbRule> create_emi_clearance_rule();
std::unique_ptr<PcbRule> create_sensitive_shield_rule();

} // namespace auditor

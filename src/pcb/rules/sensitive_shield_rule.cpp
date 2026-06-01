#include "../pcb_analyzer.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace auditor {

// 判断点是否在多边形内部 (射线交叉法)
static bool is_point_in_polygon(const Point& p, const std::vector<Point>& polygon) {
    if (polygon.empty()) return false;
    bool inside = false;
    size_t n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
            (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

class SensitiveShieldRule : public PcbRule {
public:
    std::string get_id() const override { return "PCB-02"; }
    std::string get_description() const override {
        return "敏感小信号屏蔽与侧向防护分析：核对敏感 FB 走线到强干扰开关线的侧向平行间距，以及垂直投影地完整性。";
    }

    void analyze(const SExpr& pcb, Report& report) override {
        PcbAnalyzer local_analyzer;
        local_analyzer.load_mock_pcb(pcb);

        const auto& segments = local_analyzer.get_segments();
        const auto& zones = local_analyzer.get_zones();

        // 识别敏感信号网络 (FB, Feedback, VFB 等)
        auto is_sensitive_net = [&](int net_id) -> bool {
            std::string name = local_analyzer.get_net_name(net_id);
            if (name.empty()) return false;
            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return (lower_name.find("fb") != std::string::npos ||
                    lower_name.find("feedback") != std::string::npos ||
                    lower_name.find("vfb") != std::string::npos);
        };

        // 识别强干扰开关网络
        auto is_high_frequency_net = [&](int net_id) -> bool {
            std::string name = local_analyzer.get_net_name(net_id);
            if (name.empty()) return false;
            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return (lower_name.find("sw") != std::string::npos ||
                    lower_name.find("vadj") != std::string::npos ||
                    lower_name.find("drain") != std::string::npos ||
                    lower_name.find("mos") != std::string::npos ||
                    lower_name.find("primary") != std::string::npos);
        };

        // 识别 GND 铺铜网络
        auto is_gnd_net = [&](int net_id) -> bool {
            std::string name = local_analyzer.get_net_name(net_id);
            if (name.empty()) return false;
            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            return (lower_name.find("gnd") != std::string::npos ||
                    lower_name.find("sgnd") != std::string::npos ||
                    lower_name.find("agnd") != std::string::npos ||
                    lower_name.find("dgnd") != std::string::npos ||
                    lower_name.find("pgnd") != std::string::npos);
        };

        // 收集敏感线段与开关线段
        std::vector<PcbSegment> sens_segs;
        std::vector<PcbSegment> sw_segs;

        for (const auto& seg : segments) {
            if (is_sensitive_net(seg.net)) {
                sens_segs.push_back(seg);
            } else if (is_high_frequency_net(seg.net)) {
                sw_segs.push_back(seg);
            }
        }

        // 1. 侧向平行间距检测 (同一层)
        for (const auto& ss : sens_segs) {
            for (const auto& ssw : sw_segs) {
                // 仅当在同一层时检测侧向耦合
                if (ss.layer == ssw.layer) {
                    double d1 = ss.start.distance_to_segment(ssw.start, ssw.end);
                    double d2 = ss.end.distance_to_segment(ssw.start, ssw.end);
                    double d3 = ssw.start.distance_to_segment(ss.start, ss.end);
                    double d4 = ssw.end.distance_to_segment(ss.start, ss.end);

                    double d_center = std::min({d1, d2, d3, d4});
                    double w1 = ss.width > 0.0 ? ss.width : 0.2;
                    double w2 = ssw.width > 0.0 ? ssw.width : 0.2;
                    double d_edge = std::max(0.0, d_center - (w1 + w2) / 2.0);

                    // 检查 3W 规则或 1.2mm
                    double threshold_3w = 3.0 * w1;
                    double min_allowed = std::max(threshold_3w, 1.2);

                    if (d_edge < min_allowed) {
                        std::string sens_net_name = local_analyzer.get_net_name(ss.net);
                        std::string sw_net_name = local_analyzer.get_net_name(ssw.net);
                        std::string msg = "敏感走线 Net " + sens_net_name + " 与强干扰开关线 Net " + sw_net_name + 
                                          " 同在层 " + ss.layer + " 上，侧向最小边缘间距仅为 " + std::to_string(d_edge) + 
                                          " mm，低于 3W (" + std::to_string(threshold_3w) + " mm) 及 1.2 mm 防护安全距离，存在严重的高频电容跨接耦合干扰隐患！";
                        
                        std::string loc = "Segment (" + std::to_string(ss.start.x) + "," + std::to_string(ss.start.y) + ")->(" + 
                                          std::to_string(ss.end.x) + "," + std::to_string(ss.end.y) + ")";
                        report.add_violation(get_id(), "WARNING", msg, loc);
                    }
                }
            }
        }

        // 2. 跨层投影 GND 地完整性检测
        // 对于每一条敏感走线，检查另一层上是否有完整的 GND 铺铜屏蔽
        for (const auto& ss : sens_segs) {
            std::string ref_layer = "";
            if (ss.layer == "F.Cu") {
                ref_layer = "B.Cu";
            } else if (ss.layer == "B.Cu") {
                ref_layer = "F.Cu";
            } else {
                continue; // 非表底层（或者多层板），此处我们只对常规双层表底互为参考进行严格分析
            }

            // 采样起点、终点和中点进行垂直参考地校验
            Point mid = (ss.start + ss.end) * 0.5;
            std::vector<Point> test_points = {ss.start, ss.end, mid};

            bool start_covered = false;
            bool end_covered = false;
            bool mid_covered = false;

            for (const auto& zone : zones) {
                // 仅检查在参考层上的 GND zone
                if (zone.layer == ref_layer && is_gnd_net(zone.net)) {
                    if (!start_covered && is_point_in_polygon(ss.start, zone.outline)) {
                        start_covered = true;
                    }
                    if (!end_covered && is_point_in_polygon(ss.end, zone.outline)) {
                        end_covered = true;
                    }
                    if (!mid_covered && is_point_in_polygon(mid, zone.outline)) {
                        mid_covered = true;
                    }
                }
            }

            // 只要有一个采样点没有被地铺铜覆盖，即视为地缝或无完整屏蔽，回流阻抗增加
            if (!start_covered || !end_covered || !mid_covered) {
                std::string sens_net_name = local_analyzer.get_net_name(ss.net);
                std::string msg = "敏感走线 Net " + sens_net_name + " (位于层 " + ss.layer + 
                                  ") 的跨层垂直投影区域缺乏完整的 GND 铺铜屏蔽参考面 (参考层: " + ref_layer + 
                                  ")，存在地裂缝或回流路径阻断风险，会导致信号环路面积增大与信号完整性遭受破坏！";
                
                std::string loc = "Segment (" + std::to_string(ss.start.x) + "," + std::to_string(ss.start.y) + ")->(" + 
                                  std::to_string(ss.end.x) + "," + std::to_string(ss.end.y) + ")";
                report.add_violation(get_id(), "WARNING", msg, loc);
            }
        }
    }
};

std::unique_ptr<PcbRule> create_sensitive_shield_rule() {
    return std::make_unique<SensitiveShieldRule>();
}

} // namespace auditor

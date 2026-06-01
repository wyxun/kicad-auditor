#include "../pcb_analyzer.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace auditor {

class EmiClearanceRule : public PcbRule {
public:
    std::string get_id() const override { return "PCB-01"; }
    std::string get_description() const override { 
        return "GND 地网高频共模干扰注入警告：检查高频开关实体与大面积 GND 铺铜的最小几何间距与高频耦合阻抗。"; 
    }

    void analyze(const SExpr& pcb, Report& report) override {
        // 利用解析器内部的局部实体提取逻辑来提取我们需要的数据
        PcbAnalyzer local_analyzer;
        local_analyzer.load_mock_pcb(pcb);

        const auto& segments = local_analyzer.get_segments();
        const auto& vias = local_analyzer.get_vias();
        const auto& pads = local_analyzer.get_pads();
        const auto& zones = local_analyzer.get_zones();

        // 识别高频开关网络
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

        // 识别大面积 GND 铺铜区
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

        struct HighFreqEntity {
            std::string type;   
            std::string info;   
            double length = 0.0;
            double area_factor = 0.0; 
            std::function<double(const Point&)> get_dist;
        };

        std::vector<HighFreqEntity> hf_entities;

        // 1. 提取高频 segment
        for (const auto& seg : segments) {
            if (is_high_frequency_net(seg.net)) {
                double L = seg.start.distance_to(seg.end);
                double W = seg.width > 0.0 ? seg.width : 0.25;
                std::string net_name = local_analyzer.get_net_name(seg.net);
                HighFreqEntity entity{
                    "segment",
                    "Segment (" + std::to_string(seg.start.x) + "," + std::to_string(seg.start.y) + ")->(" + std::to_string(seg.end.x) + "," + std::to_string(seg.end.y) + ") on Net " + net_name,
                    L > 0.0 ? L : 0.1, // 防止退化
                    L * W,
                    [seg](const Point& p) {
                        return seg.distance_to_point(p);
                    }
                };
                hf_entities.push_back(entity);
            }
        }

        // 2. 提取高频 via
        for (const auto& via : vias) {
            if (is_high_frequency_net(via.net)) {
                double L = M_PI * via.size; 
                double A = M_PI * (via.size / 2.0) * (via.size / 2.0); 
                std::string net_name = local_analyzer.get_net_name(via.net);
                HighFreqEntity entity{
                    "via",
                    "Via at (" + std::to_string(via.pos.x) + "," + std::to_string(via.pos.y) + ") on Net " + net_name,
                    L,
                    A,
                    [via](const Point& p) {
                        return via.distance_to_point(p);
                    }
                };
                hf_entities.push_back(entity);
            }
        }

        // 3. 提取高频 pad
        for (const auto& pad : pads) {
            if (is_high_frequency_net(pad.net)) {
                double L = 2.0 * (pad.size.x + pad.size.y); 
                double A = pad.size.x * pad.size.y; 
                std::string net_name = local_analyzer.get_net_name(pad.net);
                HighFreqEntity entity{
                    "pad",
                    "Pad " + pad.name + " at (" + std::to_string(pad.pos.x) + "," + std::to_string(pad.pos.y) + ") on Net " + net_name,
                    L,
                    A,
                    [pad](const Point& p) {
                        return pad.distance_to_point(p);
                    }
                };
                hf_entities.push_back(entity);
            }
        }

        if (hf_entities.empty()) return;

        // 收集大面积 GND zone 填充多边形的所有顶点
        std::vector<Point> gnd_vertices;
        for (const auto& zone : zones) {
            if (is_gnd_net(zone.net)) {
                for (const auto& pt : zone.outline) {
                    gnd_vertices.push_back(pt);
                }
            }
        }

        if (gnd_vertices.empty()) return;

        // 物理常数与阻抗计算
        const double f = 11.6e6; // 11.6 MHz
        const double epsilon_r = 4.4;       // FR4
        const double H_eff = 0.8; // 等效展宽耦合高度 0.8 mm
        // 共面电容系数: C (pF) = epsilon_r * 0.008854 * H_eff * L / d
        const double C_factor = epsilon_r * 0.008854 * H_eff;

        for (const auto& hfe : hf_entities) {
            double min_d = 1e9;
            Point nearest_vertex{0.0, 0.0};
            for (const auto& v : gnd_vertices) {
                double d = hfe.get_dist(v);
                if (d < min_d) {
                    min_d = d;
                    nearest_vertex = v;
                }
            }

            // 限制一个微小间距，防止零除
            if (min_d < 0.001) {
                min_d = 0.001;
            }

            double C_pF = C_factor * hfe.length / min_d;
            double C_F = C_pF * 1e-12;
            double Xc = 1.0 / (2.0 * M_PI * f * C_F);

            if (Xc < 5000.0) {
                std::string msg = "高频开关实体与GND铺铜的绝对最小几何距离为 " + 
                                  std::to_string(min_d) + " mm (最近顶点 " + 
                                  std::to_string(nearest_vertex.x) + "," + std::to_string(nearest_vertex.y) + 
                                  ")。估算对地寄生电容 C = " + std::to_string(C_pF) + 
                                  " pF，等效频宽 11.6MHz (对应30ns边沿) 下的交流耦合阻抗 Xc = " + 
                                  std::to_string(Xc / 1000.0) + " kOhm，低于安全阈值 5.0 kOhm，容易向地网注入共模电流！";
                
                report.add_violation(get_id(), "WARNING", msg, hfe.info);
            }
        }
    }
};

std::unique_ptr<PcbRule> create_emi_clearance_rule() {
    return std::make_unique<EmiClearanceRule>();
}

} // namespace auditor

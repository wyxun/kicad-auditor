#include "../sch_analyzer.hpp"
#include "../../common/rule.hpp"
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace auditor {

class IsolationRule : public SchRule {
public:
    std::string get_id() const override { return "SCH_ISO_01"; }
    std::string get_description() const override {
        return "Verify galvanic isolation between primary and secondary sides (PC817 / Transformers).";
    }

    void analyze(const SExpr& /*sch*/, Report& report) override {
        const auto* analyzer = SchAnalyzerContext::get();
        if (!analyzer) return;

        const auto& components = analyzer->get_components();
        const auto& adj = analyzer->get_adj();

        for (const auto& comp : components) {
            // 自动识别隔离器件 (光耦 U* 且 Value 含有 PC817/Opto/EL357 等；变压器 T*)
            bool is_opto = (comp.ref.rfind("U", 0) == 0) && 
                           (comp.value.find("PC817") != std::string::npos ||
                            comp.value.find("Opto") != std::string::npos ||
                            comp.value.find("EL357") != std::string::npos ||
                            comp.value.find("EL817") != std::string::npos ||
                            comp.value.find("Isolation") != std::string::npos);
            bool is_tran = (comp.ref.rfind("T", 0) == 0);

            if (!is_opto && !is_tran) continue;

            // 提取初级和次级引脚
            std::vector<const PinInfo*> pri_pins;
            std::vector<const PinInfo*> sec_pins;

            for (const auto& pin : comp.pins) {
                if (pin.name == "1" || pin.name == "2" || pin.name == "IN" || pin.name == "PRI") {
                    pri_pins.push_back(&pin);
                } else if (pin.name == "3" || pin.name == "4" || pin.name == "OUT" || pin.name == "SEC") {
                    sec_pins.push_back(&pin);
                }
            }

            if (pri_pins.empty() || sec_pins.empty()) continue;

            // 对每一个次级侧引脚运行 BFS 隔离检查
            for (const auto* sec_pin : sec_pins) {
                std::queue<std::string> q;
                std::unordered_set<std::string> visited;
                std::unordered_map<std::string, std::string> parent;

                q.push(sec_pin->pt_id);
                visited.insert(sec_pin->pt_id);

                bool isolation_failed = false;
                std::string failure_reason;
                std::string target_node;

                while (!q.empty()) {
                    std::string curr = q.front();
                    q.pop();

                    // 1. 检查是否碰到了当前器件的初级侧引脚
                    for (const auto* pri_pin : pri_pins) {
                        if (curr == pri_pin->pt_id) {
                            isolation_failed = true;
                            target_node = curr;
                            failure_reason = "Directly connected to primary pin " + pri_pin->name + " of " + comp.ref;
                            break;
                        }
                    }
                    if (isolation_failed) break;

                    // 2. 检查是否连通到了初级侧 GND 网络
                    std::string net_name = analyzer->get_net_name(curr);
                    if (net_name == "GND") {
                        isolation_failed = true;
                        target_node = curr;
                        failure_reason = "Secondary circuit connected to primary ground (GND)";
                        break;
                    }

                    // 遍历物理图连线
                    auto it = adj.find(curr);
                    if (it != adj.end()) {
                        for (const auto& next_pt : it->second) {
                            if (visited.find(next_pt) == visited.end()) {
                                visited.insert(next_pt);
                                parent[next_pt] = curr;
                                q.push(next_pt);
                            }
                        }
                    }
                }

                if (isolation_failed) {
                    // 回溯生成诊断路径
                    std::vector<std::string> path;
                    std::string temp = target_node;
                    while (temp != sec_pin->pt_id) {
                        path.push_back(temp);
                        temp = parent[temp];
                    }
                    path.push_back(sec_pin->pt_id);
                    std::reverse(path.begin(), path.end());

                    std::ostringstream path_str;
                    path_str << comp.ref << "." << sec_pin->name << "(" << sec_pin->pt_id << ")";
                    
                    const auto& pt_to_lbl = analyzer->get_point_to_label();

                    for (size_t i = 1; i < path.size(); ++i) {
                        path_str << " -> (" << path[i] << ")";
                        auto it_lbl = pt_to_lbl.find(path[i]);
                        if (it_lbl != pt_to_lbl.end()) {
                            path_str << "[" << it_lbl->second << "]";
                        }
                    }

                    std::string msg = "Isolation failure detected at " + comp.ref + " pin " + sec_pin->name + 
                                      "! " + failure_reason + ". Connection path: " + path_str.str();

                    report.add_violation(get_id(), "ERROR", msg, comp.ref);
                }
            }
        }
    }
};

std::unique_ptr<SchRule> create_isolation_rule() {
    return std::make_unique<IsolationRule>();
}

} // namespace auditor

#include "sch_analyzer.hpp"
#include <iomanip>
#include <sstream>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <iostream>

namespace auditor {

std::string SchAnalyzer::format_point(const Point& pt) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << pt.x << "," << pt.y;
    return oss.str();
}

void SchAnalyzer::clear() {
    components_.clear();
    adj_.clear();
    point_to_label_.clear();
    resolved_nets_.clear();
}

bool SchAnalyzer::load_schematic(const SExpr& sch_root) {
    clear();
    build_topology(sch_root);
    return true;
}

Report SchAnalyzer::analyze(SchRuleRegistry& registry) {
    SchAnalyzerContext::set(this);
    Report report = registry.run_all(*parse_sexpr("()"));
    SchAnalyzerContext::set(nullptr);
    return report;
}

void SchAnalyzer::build_topology(const SExpr& sch_root) {
    // 递归提取 Symbols，Wires 和 Labels
    extract_symbols(sch_root);
    extract_connections(sch_root);
}

void SchAnalyzer::extract_symbols(const SExpr& node) {
    if (node.head == "symbol") {
        ComponentInfo comp;
        
        // 提取 Reference 和 Value 属性
        for (const auto& child : node.children) {
            if (child && child->head == "property") {
                std::string_view prop_name = child->get_value_at(0);
                std::string prop_val = std::string(child->get_value_at(1));
                
                // 兼容 KiCad v6+ 的 (property "Name" (value "Val")) 结构
                if (prop_val.empty()) {
                    const auto* val_node = child->find_child("value");
                    if (val_node) {
                        prop_val = std::string(val_node->get_value_at(0));
                    }
                }

                if (prop_name == "Reference") {
                    comp.ref = prop_val;
                } else if (prop_name == "Value") {
                    comp.value = prop_val;
                }
            }
        }

        // 如果没有位号，略过
        if (!comp.ref.empty()) {
            // 提取器件的绝对位置 (at X Y ...)
            const auto* at_node = node.find_child("at");
            if (at_node) {
                comp.pos.x = at_node->get_double_at(0);
                comp.pos.y = at_node->get_double_at(1);
            }

            // 提取引脚
            for (const auto& child : node.children) {
                if (child && child->head == "pin") {
                    PinInfo pin;
                    pin.name = std::string(child->get_value_at(0));
                    
                    const auto* pin_at = child->find_child("at");
                    if (pin_at) {
                        pin.rel_pos.x = pin_at->get_double_at(0);
                        pin.rel_pos.y = pin_at->get_double_at(1);
                    }
                    
                    // 绝对坐标 = 器件位置 + 引脚相对位置
                    pin.abs_pos = comp.pos + pin.rel_pos;
                    pin.pt_id = format_point(pin.abs_pos);
                    
                    comp.pins.push_back(pin);
                }
            }

            components_.push_back(comp);
        }
    }

    // 递归遍历子节点，以防 symbol 嵌在其他结构中
    for (const auto& child : node.children) {
        if (child) {
            extract_symbols(*child);
        }
    }
}

void SchAnalyzer::extract_connections(const SExpr& node) {
    if (node.head == "wire") {
        const auto* pts_node = node.find_child("pts");
        if (pts_node) {
            std::vector<Point> pts;
            for (const auto& xy : pts_node->children) {
                if (xy && xy->head == "xy") {
                    pts.push_back({xy->get_double_at(0), xy->get_double_at(1)});
                }
            }
            if (pts.size() >= 2) {
                // 原理图中的 wire 连接端点
                for (size_t i = 0; i < pts.size() - 1; ++i) {
                    std::string u = format_point(pts[i]);
                    std::string v = format_point(pts[i + 1]);
                    adj_[u].push_back(v);
                    adj_[v].push_back(u);
                }
            }
        }
    } else if (node.head == "label" || node.head == "global_label" || node.head == "hierarchical_label") {
        std::string label_name = std::string(node.get_value_at(0));
        const auto* at_node = node.find_child("at");
        if (at_node && !label_name.empty()) {
            Point pt{at_node->get_double_at(0), at_node->get_double_at(1)};
            point_to_label_[format_point(pt)] = label_name;
        }
    }

    // 递归处理所有子节点
    for (const auto& child : node.children) {
        if (child) {
            extract_connections(*child);
        }
    }
}

std::string SchAnalyzer::get_net_name(const std::string& pt_id) const {
    // 1. 检查缓存
    auto cached = resolved_nets_.find(pt_id);
    if (cached != resolved_nets_.end()) {
        return cached->second;
    }

    // 2. 在物理邻接图上运行 BFS 以找出当前物理点所在连通块中所有的网络标签
    std::unordered_set<std::string> visited;
    std::queue<std::string> q;
    
    q.push(pt_id);
    visited.insert(pt_id);

    std::vector<std::string> labels_found;

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();

        // 检查该物理点是否有 label
        auto it_lbl = point_to_label_.find(curr);
        if (it_lbl != point_to_label_.end()) {
            labels_found.push_back(it_lbl->second);
        }

        // 遍历邻接点
        auto it_adj = adj_.find(curr);
        if (it_adj != adj_.end()) {
            for (const auto& next_pt : it_adj->second) {
                if (visited.find(next_pt) == visited.end()) {
                    visited.insert(next_pt);
                    q.push(next_pt);
                }
            }
        }
    }

    // 确定电气网络名字
    std::string resolved_name;
    if (!labels_found.empty()) {
        // 优先选择非 "Net-" 形式的显式命名 label
        auto best_lbl = labels_found.begin();
        for (auto it = labels_found.begin(); it != labels_found.end(); ++it) {
            if (it->rfind("Net-", 0) != 0) { // 不是以 "Net-" 开头
                best_lbl = it;
                break;
            }
        }
        resolved_name = *best_lbl;
    } else {
        // 默认使用该坐标作为网络名
        resolved_name = pt_id;
    }

    // 将整个连通块中的所有节点全部缓存起来，大幅加速后续查询
    for (const auto& visited_pt : visited) {
        resolved_nets_[visited_pt] = resolved_name;
    }

    return resolved_name;
}

} // namespace auditor

#include "sch_analyzer.hpp"
#include <iomanip>
#include <sstream>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <cctype>

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
    lib_symbols_.clear();
    extract_lib_symbols(sch_root);
    // 递归提取 Symbols，Wires 和 Labels
    extract_symbols(sch_root);
    extract_connections(sch_root);
}

void SchAnalyzer::extract_lib_symbols(const SExpr& node) {
    if (node.head == "lib_symbols") {
        for (const auto& sym_node : node.children) {
            if (sym_node && sym_node->head == "symbol") {
                std::string lib_id = std::string(sym_node->get_value_at(0));
                if (lib_id.empty() && !sym_node->children.empty()) {
                    lib_id = std::string(sym_node->children[0]->value);
                }
                
                std::vector<LibPinInfo> pins;
                collect_lib_pins(*sym_node, pins);
                lib_symbols_[lib_id] = pins;
            }
        }
    }
    
    for (const auto& child : node.children) {
        if (child) {
            extract_lib_symbols(*child);
        }
    }
}

void SchAnalyzer::collect_lib_pins(const SExpr& node, std::vector<LibPinInfo>& pins) {
    if (node.head == "pin") {
        LibPinInfo pin;
        const auto* num_node = node.find_child("number");
        if (num_node) {
            pin.number = std::string(num_node->get_value_at(0));
        }
        const auto* name_node = node.find_child("name");
        if (name_node) {
            pin.name = std::string(name_node->get_value_at(0));
        }
        const auto* at_node = node.find_child("at");
        if (at_node) {
            pin.rel_pos.x = at_node->get_double_at(0);
            pin.rel_pos.y = at_node->get_double_at(1);
        }
        if (!pin.number.empty()) {
            pins.push_back(pin);
        }
    }
    
    for (const auto& child : node.children) {
        if (child) {
            collect_lib_pins(*child, pins);
        }
    }
}

void SchAnalyzer::extract_symbols(const SExpr& node) {
    if (node.head == "symbol") {
        ComponentInfo comp;
        
        // 提取 Reference 和 Value 属性
        for (const auto& child : node.children) {
            if (child && child->head == "property") {
                std::string_view prop_name = child->get_value_at(0);
                if (!prop_name.empty()) {
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

                    comp.properties[std::string(prop_name)] = prop_val;
                }
            }
        }

        // 如果没有位号，略过
        if (!comp.ref.empty()) {
            // 提取器件的绝对位置 (at X Y Angle)
            double inst_angle = 0.0;
            const auto* at_node = node.find_child("at");
            if (at_node) {
                comp.pos.x = at_node->get_double_at(0);
                comp.pos.y = at_node->get_double_at(1);
                if (at_node->children.size() >= 3) {
                    inst_angle = at_node->get_double_at(2);
                }
            }

            bool is_mirror_y = false;
            const auto* mirror_node = node.find_child("mirror");
            if (mirror_node && mirror_node->get_value_at(0) == "y") {
                is_mirror_y = true;
            }

            std::string lib_id;
            const auto* lib_id_node = node.find_child("lib_id");
            if (lib_id_node) {
                lib_id = std::string(lib_id_node->get_value_at(0));
            }

            // 1. 尝试从 lib_symbols 库定义中提取引脚并映射坐标
            bool found_in_lib = false;
            auto lib_it = lib_symbols_.find(lib_id);
            if (!lib_id.empty() && lib_it != lib_symbols_.end() && !lib_it->second.empty()) {
                found_in_lib = true;
                double angle_rad = inst_angle * M_PI / 180.0;
                double cos_a = std::cos(angle_rad);
                double sin_a = std::sin(angle_rad);

                for (const auto& lib_pin : lib_it->second) {
                    double dx = lib_pin.rel_pos.x;
                    double dy = lib_pin.rel_pos.y;

                    if (is_mirror_y) {
                        dx = -dx;
                    }

                    // 旋转并转换坐标系 (Y轴反转)
                    double dx_rot = dx * cos_a + dy * sin_a;
                    double dy_rot = dx * sin_a - dy * cos_a;

                    Point abs_pos{ comp.pos.x + dx_rot, comp.pos.y + dy_rot };

                    // 注册引脚编号 (Pin Number)
                    if (!lib_pin.number.empty()) {
                        PinInfo pin;
                        pin.name = lib_pin.number;
                        pin.rel_pos = { dx, dy };
                        pin.abs_pos = abs_pos;
                        pin.pt_id = format_point(abs_pos);
                        comp.pins.push_back(pin);
                    }

                    // 注册引脚名称 (Pin Name)
                    if (!lib_pin.name.empty() && lib_pin.name != lib_pin.number) {
                        PinInfo pin;
                        pin.name = lib_pin.name;
                        pin.rel_pos = { dx, dy };
                        pin.abs_pos = abs_pos;
                        pin.pt_id = format_point(abs_pos);
                        comp.pins.push_back(pin);
                    }
                }
            }

            // 2. 如果库中没有，降级为原有的引脚解析逻辑 (Mock 原理图直接定位)
            if (!found_in_lib) {
                for (const auto& child : node.children) {
                    if (child && child->head == "pin") {
                        PinInfo pin;
                        pin.name = std::string(child->get_value_at(0));
                        
                        const auto* pin_at = child->find_child("at");
                        if (pin_at) {
                            pin.rel_pos.x = pin_at->get_double_at(0);
                            pin.rel_pos.y = pin_at->get_double_at(1);
                        }
                        
                        pin.abs_pos = comp.pos + pin.rel_pos;
                        pin.pt_id = format_point(pin.abs_pos);
                        
                        comp.pins.push_back(pin);
                    }
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

ComponentAnalysisResult SchAnalyzer::analyze_component(const std::string& ref, double nearby_radius) const {
    ComponentAnalysisResult result;
    
    const ComponentInfo* target_comp = nullptr;
    for (const auto& comp : components_) {
        if (comp.ref == ref) {
            target_comp = &comp;
            break;
        }
    }

    if (!target_comp) {
        result.found = false;
        return result;
    }

    result.found = true;
    result.ref = target_comp->ref;
    result.value = target_comp->value;
    result.pos = target_comp->pos;
    result.properties = target_comp->properties;

    // 根据绝对坐标对 pins 进行去重合并并提取电气连接关系
    std::vector<std::string> processed_pts;
    for (const auto& pin : target_comp->pins) {
        if (std::find(processed_pts.begin(), processed_pts.end(), pin.pt_id) != processed_pts.end()) {
            continue;
        }
        processed_pts.push_back(pin.pt_id);

        PinConnectionInfo upin;
        upin.net_name = get_net_name(pin.pt_id);

        // 寻找同一个位置的所有别名
        for (const auto& alias_pin : target_comp->pins) {
            if (alias_pin.pt_id == pin.pt_id) {
                bool is_num = true;
                for (char c : alias_pin.name) {
                    if (!std::isdigit(c)) { is_num = false; break; }
                }
                if (is_num) {
                    upin.pin_num = alias_pin.name;
                } else {
                    upin.pin_name = alias_pin.name;
                }
            }
        }
        // 兜底处理
        if (upin.pin_num.empty() && !upin.pin_name.empty()) {
            upin.pin_num = upin.pin_name;
            upin.pin_name = "";
        }

        // 跑 BFS 找这个 net 上的其它直连器件
        std::unordered_set<std::string> visited;
        std::queue<std::string> q;
        q.push(pin.pt_id);
        visited.insert(pin.pt_id);

        while (!q.empty()) {
            std::string curr = q.front();
            q.pop();

            // 查其它引脚
            for (const auto& other_comp : components_) {
                if (other_comp.ref == ref) continue; // 排除自己
                for (const auto& other_pin : other_comp.pins) {
                    if (other_pin.pt_id == curr) {
                        bool exists = false;
                        for (const auto& conn : upin.other_connections) {
                            if (conn.first == other_comp.ref && conn.second == other_pin.name) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists) {
                            upin.other_connections.push_back({other_comp.ref, other_pin.name});
                        }
                    }
                }
            }

            // 物理 BFS 扩展
            auto adj_it = adj_.find(curr);
            if (adj_it != adj_.end()) {
                for (const auto& next_pt : adj_it->second) {
                    if (visited.find(next_pt) == visited.end()) {
                        visited.insert(next_pt);
                        q.push(next_pt);
                    }
                }
            }
        }

        result.pins.push_back(upin);
    }

    // 周边邻近器件搜索及排序
    for (const auto& other : components_) {
        if (other.ref == ref) continue;
        double dist = target_comp->pos.distance_to(other.pos);
        if (dist <= nearby_radius) {
            result.nearby_components.push_back({other.ref, other.value, dist});
        }
    }
    std::sort(result.nearby_components.begin(), result.nearby_components.end(), [](const NearbyComponentInfo& a, const NearbyComponentInfo& b) {
        return a.distance < b.distance;
    });

    return result;
}

} // namespace auditor

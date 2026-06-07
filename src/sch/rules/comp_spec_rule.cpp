#include "../sch_analyzer.hpp"
#include "../../common/rule.hpp"
#include "../../common/json.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>

namespace auditor {

class CompSpecRule : public SchRule {
public:
    std::string get_id() const override { return "SCH_COMP_01"; }
    std::string get_description() const override {
        return "Verify component electrical ratings (overvoltage) and report missing database specs.";
    }

    void analyze(const SExpr& /*sch*/, Report& report) override {
        const auto* analyzer = SchAnalyzerContext::get();
        if (!analyzer) return;

        // 1. 加载本地规格数据库 data/components_db.json
        std::ifstream file("data/components_db.json");
        std::unordered_map<std::string, JsonValue> lcsc_db;
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            JsonValue db_root = parse_json(buffer.str());
            if (db_root.is_object()) {
                auto comps = db_root["components"];
                if (comps.is_array()) {
                    for (const auto& item : comps.arr_val) {
                        if (item.is_object()) {
                            std::string lcsc_id = item["lcsc_id"].str_val;
                            if (!lcsc_id.empty()) {
                                lcsc_db[lcsc_id] = item;
                            }
                        }
                    }
                }
            }
        }

        const auto& components = analyzer->get_components();

        // 2. 遍历所有器件进行规格匹配与电压/功耗核算
        for (const auto& comp : components) {
            // A. LCSC ID 寻找
            std::string lcsc_id;
            auto it_lcsc = comp.properties.find("LCSC Part");
            if (it_lcsc != comp.properties.end()) {
                lcsc_id = it_lcsc->second;
            } else {
                it_lcsc = comp.properties.find("LCSC");
                if (it_lcsc != comp.properties.end()) {
                    lcsc_id = it_lcsc->second;
                }
            }

            if (!lcsc_id.empty()) {
                auto it_db = lcsc_db.find(lcsc_id);
                if (it_db == lcsc_db.end()) {
                    // A. 差集缺失盘点 (INFO级)
                    report.add_violation(get_id(), "INFO",
                        "Component " + comp.ref + " (LCSC Part: " + lcsc_id + ") specification details not found in data/components_db.json.",
                        comp.ref);
                } else {
                    // B. 耐压超压校验
                    const auto& db_item = it_db->second;
                    double max_volt = db_item["max_voltage"].num_val;

                    // 智能推导各引脚连接网络的最大实际工作电压
                    double max_actual_volt = 0.0;
                    bool volt_detected = false;

                    for (const auto& pin : comp.pins) {
                        std::string net_name = analyzer->get_net_name(pin.pt_id);
                        double volt = extract_voltage_from_net(net_name);
                        if (volt >= 0.0) {
                            volt_detected = true;
                            if (volt > max_actual_volt) {
                                max_actual_volt = volt;
                            }
                        }
                    }

                    if (volt_detected && max_volt != 0.0 && std::abs(max_actual_volt) > std::abs(max_volt)) {
                        std::ostringstream oss;
                        oss << "Component " << comp.ref << " (Value: " << comp.value 
                            << ", LCSC Part: " << lcsc_id << ") maximum voltage rating is " << max_volt 
                            << "V, but it is connected to a network with measured voltage up to " 
                            << max_actual_volt << "V, causing severe overvoltage fire risk!";
                        report.add_violation(get_id(), "ERROR", oss.str(), comp.ref);
                    }
                }
            }

            // B. 电阻封装静态发热功耗校验
            if (comp.ref.rfind("R", 0) == 0 && comp.pins.size() >= 2) {
                std::string net1 = analyzer->get_net_name(comp.pins[0].pt_id);
                std::string net2 = analyzer->get_net_name(comp.pins[1].pt_id);
                double v1 = extract_voltage_from_net(net1);
                double v2 = extract_voltage_from_net(net2);

                if (v1 >= 0.0 && v2 >= 0.0) {
                    double delta_u = std::abs(v1 - v2);
                    // 匹配 Footprint 获取 P_max
                    double p_max = -1.0;
                    auto it_fp = comp.properties.find("Footprint");
                    if (it_fp != comp.properties.end()) {
                        std::string fp = it_fp->second;
                        std::transform(fp.begin(), fp.end(), fp.begin(), ::tolower);
                        if (fp.find("0402") != std::string::npos) p_max = 0.0625;
                        else if (fp.find("0603") != std::string::npos) p_max = 0.1000;
                        else if (fp.find("0805") != std::string::npos) p_max = 0.1250;
                        else if (fp.find("1206") != std::string::npos) p_max = 0.2500;
                    }

                    double r_val = parse_resistor_value(comp.value);
                    if (r_val > 0.0 && p_max > 0.0) {
                        double p = (delta_u * delta_u) / r_val;
                        if (p > p_max) {
                            std::ostringstream oss;
                            oss.precision(4);
                            oss << "Component " << comp.ref << " (Value: " << comp.value 
                                << ", Footprint: " << (it_fp != comp.properties.end() ? it_fp->second : "unknown")
                                << ") calculated power dissipation is " << p << "W, which exceeds its package limit of " 
                                << p_max << "W, causing severe overheating fire risk!";
                            report.add_violation(get_id(), "ERROR", oss.str(), comp.ref);
                        }
                    }
                }
            }

            // C. 大电流回路载流裕度校验 (电源芯片 SW/LX 开关节点)
            if (comp.ref.rfind("U", 0) == 0) {
                const PinInfo* sw_pin = nullptr;
                for (const auto& pin : comp.pins) {
                    std::string pin_name_lower = pin.name;
                    std::transform(pin_name_lower.begin(), pin_name_lower.end(), pin_name_lower.begin(), ::tolower);
                    if (pin_name_lower == "sw" || pin_name_lower == "lx") {
                        sw_pin = &pin;
                        break;
                    }
                }

                if (sw_pin) {
                    std::string sw_net = analyzer->get_net_name(sw_pin->pt_id);
                    // 查芯片最大输出电流
                    double i_source = 0.0;
                    std::string u_lcsc_id;
                    auto it_u_lcsc = comp.properties.find("LCSC Part");
                    if (it_u_lcsc != comp.properties.end()) u_lcsc_id = it_u_lcsc->second;
                    else {
                        it_u_lcsc = comp.properties.find("LCSC");
                        if (it_u_lcsc != comp.properties.end()) u_lcsc_id = it_u_lcsc->second;
                    }

                    if (!u_lcsc_id.empty()) {
                        auto it_u_db = lcsc_db.find(u_lcsc_id);
                        if (it_u_db != lcsc_db.end()) {
                            i_source = it_u_db->second["max_current"].num_val;
                        }
                    }

                    if (i_source > 0.0) {
                        // 寻找连接到 sw_net 上的电感和二极管
                        for (const auto& other_comp : components) {
                            bool is_inductor = (other_comp.ref.rfind("L", 0) == 0);
                            bool is_diode = (other_comp.ref.rfind("D", 0) == 0);
                            if (!is_inductor && !is_diode) continue;

                            // 检查是否连接在 sw_net
                            bool connected_to_sw = false;
                            for (const auto& other_pin : other_comp.pins) {
                                if (analyzer->get_net_name(other_pin.pt_id) == sw_net) {
                                    connected_to_sw = true;
                                    break;
                                }
                            }

                            if (connected_to_sw) {
                                std::string other_lcsc_id;
                                auto it_other_lcsc = other_comp.properties.find("LCSC Part");
                                if (it_other_lcsc != other_comp.properties.end()) other_lcsc_id = it_other_lcsc->second;
                                else {
                                    it_other_lcsc = other_comp.properties.find("LCSC");
                                    if (it_other_lcsc != other_comp.properties.end()) other_lcsc_id = it_other_lcsc->second;
                                }

                                if (!other_lcsc_id.empty()) {
                                    auto it_other_db = lcsc_db.find(other_lcsc_id);
                                    if (it_other_db != lcsc_db.end()) {
                                        double max_curr = it_other_db->second["max_current"].num_val;
                                        if (max_curr > 0.0 && max_curr < i_source) {
                                            std::ostringstream oss;
                                            if (is_inductor) {
                                                oss << "Inductor " << other_comp.ref << " saturation current rating (" << max_curr 
                                                    << "A) is less than power IC " << comp.ref << " maximum sourcing current (" 
                                                    << i_source << "A), risking magnetic saturation and overheating!";
                                            } else {
                                                oss << "Diode " << other_comp.ref << " maximum current rating (" << max_curr 
                                                    << "A) is less than power IC " << comp.ref << " maximum sourcing current (" 
                                                    << i_source << "A), risking component damage!";
                                            }
                                            report.add_violation(get_id(), "ERROR", oss.str(), other_comp.ref);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // D. 高阻抗反馈噪声校核
            if (comp.ref.rfind("U", 0) == 0) {
                const PinInfo* fb_pin = comp.find_pin("FB");
                if (fb_pin) {
                    std::string fb_net = analyzer->get_net_name(fb_pin->pt_id);
                    const ComponentInfo* r_up = nullptr;
                    const ComponentInfo* r_down = nullptr;

                    for (const auto& r_comp : components) {
                        if (r_comp.ref.rfind("R", 0) != 0 || r_comp.pins.size() < 2) continue;

                        std::string net1 = analyzer->get_net_name(r_comp.pins[0].pt_id);
                        std::string net2 = analyzer->get_net_name(r_comp.pins[1].pt_id);

                        if (net1 == fb_net) {
                            if (net2.find("GND") != std::string::npos || net2.find("gnd") != std::string::npos) {
                                r_down = &r_comp;
                            } else {
                                r_up = &r_comp;
                            }
                        } else if (net2 == fb_net) {
                            if (net1.find("GND") != std::string::npos || net1.find("gnd") != std::string::npos) {
                                r_down = &r_comp;
                            } else {
                                r_up = &r_comp;
                            }
                        }
                    }

                    if (r_up && r_down) {
                        double r_up_val = parse_resistor_value(r_up->value);
                        double r_down_val = parse_resistor_value(r_down->value);
                        if (r_up_val > 0.0 && r_down_val > 0.0) {
                            double r_eq = (r_up_val * r_down_val) / (r_up_val + r_down_val);
                            if (r_eq > 100000.0) {
                                std::ostringstream oss;
                                oss.precision(4);
                                oss << "Feedback equivalent impedance on " << comp.ref << " is " << (r_eq / 1000.0)
                                    << " kOhm (R_up=" << r_up->ref << ":" << r_up->value << ", R_down=" << r_down->ref 
                                    << ":" << r_down->value << "), which is too high (> 100 kOhm) and vulnerable to noise. "
                                    << "Consider adding a parallel feedforward capacitor (Cff) on the upper resistor.";
                                report.add_violation(get_id(), "WARNING", oss.str(), comp.ref);
                            }
                        }
                    }
                }
            }
        }
    }

private:
    double extract_voltage_from_net(const std::string& net_name) {
        std::string s = net_name;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        // 识别 GND 标号
        if (s == "gnd" || s == "sgnd" || s == "pe" || s.find("ground") != std::string::npos) {
            return 0.0;
        }

        double volt = -1.0;
        std::string num_str;
        bool in_num = false;

        for (size_t i = 0; i < s.length(); ++i) {
            char c = s[i];
            // 将 'v', 'p', 'd' 判定为小数点 (如 3v3 -> 3.3, 12v -> 12.0)
            if (std::isdigit(c) || c == '.' || c == 'p' || c == 'd' || c == 'v') {
                if (!in_num && (c == '.' || c == 'p' || c == 'd' || c == 'v')) {
                    continue; // 小数点符号开头跳过
                }
                in_num = true;
                if (c == 'p' || c == 'd' || c == 'v') {
                    num_str += '.';
                } else {
                    num_str += c;
                }
            } else {
                if (in_num) {
                    break; // 数字提取结束
                }
            }
        }

        if (!num_str.empty()) {
            if (num_str.back() == '.') {
                num_str.pop_back();
            }
            try {
                volt = std::stod(num_str);
            } catch (...) {
                volt = -1.0;
            }
        }

        if (volt <= 0.0 || volt > 1000.0) {
            volt = -1.0;
        }
        return volt;
    }

    double parse_resistor_value(const std::string& val_str) {
        std::string s = val_str;
        // 去处所有空格，并转为小写
        s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        double multiplier = 1.0;
        size_t pos_m = s.find('m');
        size_t pos_k = s.find('k');
        size_t pos_r = s.find('r');

        if (pos_m != std::string::npos) {
            multiplier = 1000000.0;
            // 兼容 "2m2" 格式为 "2.2"
            if (pos_m > 0 && pos_m < s.length() - 1 && std::isdigit(s[pos_m - 1]) && std::isdigit(s[pos_m + 1])) {
                s[pos_m] = '.';
            } else {
                s.erase(pos_m, 1);
            }
        } else if (pos_k != std::string::npos) {
            multiplier = 1000.0;
            // 兼容 "4k7" 格式为 "4.7"
            if (pos_k > 0 && pos_k < s.length() - 1 && std::isdigit(s[pos_k - 1]) && std::isdigit(s[pos_k + 1])) {
                s[pos_k] = '.';
            } else {
                s.erase(pos_k, 1);
            }
        } else if (pos_r != std::string::npos) {
            multiplier = 1.0;
            // 兼容 "4r7" 格式为 "4.7"
            if (pos_r > 0 && pos_r < s.length() - 1 && std::isdigit(s[pos_r - 1]) && std::isdigit(s[pos_r + 1])) {
                s[pos_r] = '.';
            } else {
                s.erase(pos_r, 1);
            }
        }

        try {
            return std::stod(s) * multiplier;
        } catch (...) {
            return 0.0;
        }
    }
};

std::unique_ptr<SchRule> create_comp_spec_rule() {
    return std::make_unique<CompSpecRule>();
}

} // namespace auditor

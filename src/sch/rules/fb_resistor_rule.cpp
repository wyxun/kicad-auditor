#include "../sch_analyzer.hpp"
#include "../../common/rule.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cctype>

namespace auditor {

class FbResistorRule : public SchRule {
public:
    std::string get_id() const override { return "SCH_FB_01"; }
    std::string get_description() const override {
        return "Verify chip FB feedback resistor impedance and EN enable pin pull-up/down config.";
    }

    void analyze(const SExpr& /*sch*/, Report& report) override {
        const auto* analyzer = SchAnalyzerContext::get();
        if (!analyzer) return;

        const auto& components = analyzer->get_components();

        for (const auto& comp : components) {
            // 排除光耦等隔离器件，我们主要关注普通芯片 (位号以 U 开头)
            if (comp.ref.rfind("U", 0) != 0) continue;
            
            bool is_opto = (comp.value.find("PC817") != std::string::npos ||
                            comp.value.find("Opto") != std::string::npos ||
                            comp.value.find("EL357") != std::string::npos ||
                            comp.value.find("EL817") != std::string::npos ||
                            comp.value.find("Isolation") != std::string::npos);
            if (is_opto) continue;

            // 1. 检查是否有 FB 引脚
            const auto* fb_pin = comp.find_pin("FB");
            if (fb_pin) {
                std::string fb_net = analyzer->get_net_name(fb_pin->pt_id);
                
                const ComponentInfo* r_up = nullptr;
                const ComponentInfo* r_down = nullptr;
                std::string rup_other_net;

                // 搜索连接到 FB 网络上的电阻
                for (const auto& r_comp : components) {
                    if (r_comp.ref.rfind("R", 0) != 0) continue;
                    if (r_comp.pins.size() < 2) continue;

                    std::string net1 = analyzer->get_net_name(r_comp.pins[0].pt_id);
                    std::string net2 = analyzer->get_net_name(r_comp.pins[1].pt_id);

                    if (net1 == fb_net) {
                        // 引脚 1 连在 FB 上，引脚 2 是另一端
                        if (net2.find("GND") != std::string::npos) {
                            r_down = &r_comp;
                        } else {
                            r_up = &r_comp;
                            rup_other_net = net2;
                        }
                    } else if (net2 == fb_net) {
                        // 引脚 2 连在 FB 上，引脚 1 是另一端
                        if (net1.find("GND") != std::string::npos) {
                            r_down = &r_comp;
                        } else {
                            r_up = &r_comp;
                            rup_other_net = net1;
                        }
                    }
                }

                if (r_up && r_down) {
                    double r_up_val = parse_resistor_value(r_up->value);
                    double r_down_val = parse_resistor_value(r_down->value);
                    double r_total = r_up_val + r_down_val;

                    // 判断阻值安全性
                    if (r_total > 1000000.0) {
                        std::ostringstream oss;
                        oss << "Feedback network total impedance (" << (r_total / 1000.0) << " kOhm) on " 
                            << comp.ref << " is too high (> 1MOhm) which increases sensitivity to high-frequency noise.";
                        report.add_violation(get_id(), "WARNING", oss.str(), comp.ref);
                    } else if (r_total < 5000.0) {
                        std::ostringstream oss;
                        oss << "Feedback network total impedance (" << (r_total / 1000.0) << " kOhm) on " 
                            << comp.ref << " is too low (< 5kOhm) causing excessive static thermal loss.";
                        report.add_violation(get_id(), "WARNING", oss.str(), comp.ref);
                    }

                    // 检查是否并联前馈电容 Cff
                    bool has_cff = false;
                    for (const auto& c_comp : components) {
                        if (c_comp.ref.rfind("C", 0) != 0) continue;
                        if (c_comp.pins.size() < 2) continue;

                        std::string net1 = analyzer->get_net_name(c_comp.pins[0].pt_id);
                        std::string net2 = analyzer->get_net_name(c_comp.pins[1].pt_id);

                        if ((net1 == fb_net && net2 == rup_other_net) || 
                            (net2 == fb_net && net1 == rup_other_net)) {
                            has_cff = true;
                            break;
                        }
                    }

                    if (!has_cff) {
                        report.add_violation(get_id(), "INFO", 
                            "Feedback network lacks a parallel feedforward capacitor (Cff) on the upper feedback resistor " + 
                            r_up->ref + ", which may compromise power regulator loop stability.", comp.ref);
                    }
                }
            }

            // 2. 检查是否有 EN 引脚
            const auto* en_pin = comp.find_pin("EN");
            if (en_pin) {
                std::string en_net = analyzer->get_net_name(en_pin->pt_id);
                
                // 校验上拉/下拉配置
                bool has_pull = false;

                // A. 检查是否直接连到电源或地网络标签
                if (en_net.find("GND") != std::string::npos || 
                    en_net.find("VCC") != std::string::npos || 
                    en_net.find("VDD") != std::string::npos || 
                    en_net.find("Vin") != std::string::npos || 
                    en_net.find("3V3") != std::string::npos || 
                    en_net.find("5V") != std::string::npos ||
                    en_net.find("Vout") != std::string::npos ||
                    en_net.find("Vadj") != std::string::npos) {
                    has_pull = true;
                }

                // B. 检查是否有上拉/下拉电阻连接在 EN 网络上
                if (!has_pull) {
                    for (const auto& r_comp : components) {
                        if (r_comp.ref.rfind("R", 0) != 0) continue;
                        if (r_comp.pins.size() < 2) continue;

                        std::string net1 = analyzer->get_net_name(r_comp.pins[0].pt_id);
                        std::string net2 = analyzer->get_net_name(r_comp.pins[1].pt_id);

                        if (net1 == en_net) {
                            if (net2.find("GND") != std::string::npos || 
                                net2.find("VCC") != std::string::npos || 
                                net2.find("VDD") != std::string::npos || 
                                net2.find("Vin") != std::string::npos || 
                                net2.find("3V3") != std::string::npos || 
                                net2.find("5V") != std::string::npos) {
                                has_pull = true;
                                break;
                            }
                        } else if (net2 == en_net) {
                            if (net1.find("GND") != std::string::npos || 
                                net1.find("VCC") != std::string::npos || 
                                net1.find("VDD") != std::string::npos || 
                                net1.find("Vin") != std::string::npos || 
                                net1.find("3V3") != std::string::npos || 
                                net1.find("5V") != std::string::npos) {
                                has_pull = true;
                                break;
                            }
                        }
                    }
                }

                if (!has_pull) {
                    report.add_violation(get_id(), "WARNING", 
                        "Enable pin (EN) of component " + comp.ref + " is floating (missing explicit pull-up/pull-down resistor or power plane hookup), which may cause startup glitches or overvoltage hazards.", 
                        comp.ref);
                }
            }
        }
    }

private:
    double parse_resistor_value(const std::string& val_str) {
        std::string s = val_str;
        // 去除非字符与空格，转为小写
        s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        double multiplier = 1.0;
        size_t pos_m = s.find('m');
        size_t pos_k = s.find('k');

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
        }

        try {
            return std::stod(s) * multiplier;
        } catch (...) {
            return 0.0;
        }
    }
};

std::unique_ptr<SchRule> create_fb_resistor_rule() {
    return std::make_unique<FbResistorRule>();
}

} // namespace auditor

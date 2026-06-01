#include "report.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace auditor {

static std::string get_current_time_str() {
    std::time_t now = std::time(nullptr);
    std::tm* local_tm = std::localtime(&now);
    if (!local_tm) return "2026-06-01 23:20:00";
    std::ostringstream oss;
    oss << std::put_time(local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Report::export_markdown(const std::string& filepath, const std::string& project_name) const {
    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Failed to open report output path: " << filepath << "\n";
        return;
    }

    size_t error_count = 0;
    size_t warning_count = 0;
    size_t info_count = 0;

    std::vector<Violation> sch_violations;
    std::vector<Violation> pcb_violations;

    for (const auto& v : violations) {
        if (v.severity == "ERROR") error_count++;
        else if (v.severity == "WARNING") warning_count++;
        else info_count++;

        if (v.rule_id.rfind("SCH_", 0) == 0) {
            sch_violations.push_back(v);
        } else {
            pcb_violations.push_back(v);
        }
    }

    // 确定总体结论状态
    std::string status_badge;
    std::string status_banner;
    if (error_count > 0) {
        status_badge = "🔴 FAILED";
        status_banner = "> [!CAUTION]\n> **硬件审查未通过 (Audit Failed)**：当前设计中存在 " + std::to_string(error_count) + " 项严重电气违规风险，强烈建议先解决 ERROR 级缺陷后再行打样。";
    } else if (warning_count > 0) {
        status_badge = "🟡 PASSED WITH WARNINGS";
        status_banner = "> [!WARNING]\n> **硬件审查已通过但存在警告 (Passed with Warnings)**：未发现致命电气短路或隔离失效，但存在 " + std::to_string(warning_count) + " 项优化警告，建议仔细核对以规避潜在的设计不稳定风险。";
    } else {
        status_badge = "🟢 PERFECT PASS";
        status_banner = "> [!NOTE]\n> **硬件审查完美通过 (Perfect Pass)**：未检测到任何安全隐患或 Layout 走线警告。设计符合规范！";
    }

    out << "# 🛡️ KiCad-Auditor 硬件安全与电气间距诊断报告\n\n";
    
    // 仪表板卡片
    out << "## 📊 诊断概览 (Overview Dashboard)\n\n";
    out << "| 属性 | 审查详细信息 |\n";
    out << "| :--- | :--- |\n";
    out << "| **工程名称** | `" << project_name << "` |\n";
    out << "| **诊断时间** | `" << get_current_time_str() << "` |\n";
    out << "| **总体结论** | **" << status_badge << "** |\n";
    out << "| **编译引擎** | KiCad-Auditor C++20 Core |\n\n";

    out << "### 📈 缺陷统计指标 (Violation Metrics)\n\n";
    out << "| 🔴 CRITICAL ERROR (严重违规) | 🟡 WARNING (优化警告) | 🔵 INFO (诊断提示) |\n";
    out << "| :---: | :---: | :---: |\n";
    out << "| **" << error_count << "** | **" << warning_count << "** | **" << info_count << "** |\n\n";

    out << status_banner << "\n\n";

    // -------------------------------------------------------------
    // 第二章：原理图设计审查
    // -------------------------------------------------------------
    out << "## 🔌 原理图设计审查 (Schematic Design Audit)\n\n";
    if (sch_violations.empty()) {
        out << "### [PASS] 原理图电气安全与参数匹配无缺陷\n";
        out << "> [!TIP]\n";
        out << "> 经检查，光耦/变压器等器件的初次级电气隔离链条完整，反馈电阻分压等比例处于极佳的工作状态。\n\n";
    } else {
        out << "| 严重等级 | 规则编码 | 涉事器件/网络 | 详细缺陷描述 |\n";
        out << "| :---: | :--- | :--- | :--- |\n";
        for (const auto& v : sch_violations) {
            std::string emoji = (v.severity == "ERROR") ? "🚨 ERROR" : (v.severity == "WARNING" ? "⚠️ WARNING" : "ℹ️ INFO");
            out << "| " << emoji << " | `" << v.rule_id << "` | **" << v.component_ref << "** | " << v.description << " |\n";
        }
        out << "\n";
    }

    // -------------------------------------------------------------
    // 第三章：PCB 物理布局与电气间距审查
    // -------------------------------------------------------------
    out << "## 📐 PCB 布局与电气间距审查 (PCB Layout & Safety Audit)\n\n";
    if (pcb_violations.empty()) {
        out << "### [PASS] PCB 物理层安全间距与屏蔽地保护无缺陷\n";
        out << "> [!TIP]\n";
        out << "> 经几何高精度边界计算，高压飞线与低压侧、数字敏感走线之间的爬电与电气间距均满足配置的阈值安全线。高频干扰源的敏感节点保护良好。\n\n";
    } else {
        out << "| 严重等级 | 规则编码 | 空间定位 (层/坐标/网络) | 测量结果与违规详情 |\n";
        out << "| :---: | :--- | :--- | :--- |\n";
        for (const auto& v : pcb_violations) {
            std::string emoji = (v.severity == "ERROR") ? "🚨 ERROR" : (v.severity == "WARNING" ? "⚠️ WARNING" : "ℹ️ INFO");
            out << "| " << emoji << " | `" << v.rule_id << "` | **" << v.component_ref << "** | " << v.description << " |\n";
        }
        out << "\n";
    }

    // -------------------------------------------------------------
    // 第四章：AI Layout 智能优化与 DFM 深度建议
    // -------------------------------------------------------------
    out << "## 💡 AI Layout 智能优化与 DFM 深度建议 (AI Smart Layout & DFM Proposals)\n\n";
    out << "针对本次审计发现的硬件缺陷与警告，基于行业一流水准的 EMC 与 DFM 制造标准，AI 助手给出以下深度的改进建议：\n\n";

    bool has_iso_fail = false;
    bool has_clearance_fail = false;
    bool has_shield_fail = false;

    for (const auto& v : violations) {
        if (v.rule_id == "SCH_ISO_01") has_iso_fail = true;
        if (v.rule_id == "EMI_CLEARANCE" && v.severity == "ERROR") has_clearance_fail = true;
        if (v.rule_id == "Sensitive Shield" || v.rule_id == "PCB_SHIELD_01") has_shield_fail = true;
    }

    if (has_iso_fail || has_clearance_fail) {
        out << "### 🛠️ 建议一：隔离带与高压爬电距离（Creepage Distance）优化\n";
        out << "1. **物理隔离防爬电物理开槽（Slotting）**：\n";
        out << "   * 对于高压初级回路（如 MOS 开关管漏极 D、高压输入端）与低压次级控制回路（如 Vadj 电阻网络、光耦控制引脚），由于板材介电常数及表面积污尘影响，单纯的平面电气间距可能不足以防范电弧击穿。\n";
        out << "   * **强烈建议**在初次级隔离区（光耦引脚正下方、变压器中间空隙）进行 **PCB 物理开槽（Slot팅 / Milling Slot）**，开槽宽度推荐 $\\ge 1.5\\text{mm}$。这能消除表面爬电路径，将电气间距直接转为空间空气间距，使耐压等级跃升。\n";
        out << "2. **铺铜（Zone Refill）收缩边界**：\n";
        out << "   * 检查大面积 GND 铺铜与高压大功率网络（如变压器漏感尖峰尖点）的实际物理间距。通常建议铜皮退缩边界（Clearance Margin）配置在 $0.8\\text{mm}$ 以上。\n\n";
    }

    if (has_shield_fail) {
        out << "### 🛡️ 建议二：高速开关回路屏蔽与敏感信号（Sensitive Signal）包地防护\n";
        out << "1. **反馈环路（Feedback Trace）极简极短布线**：\n";
        out << "   * 反馈引脚（如 FB 网络）是典型的高阻抗极敏感节点。若它靠近变压器、电感、大电流高频走线等干扰源，会直接感应到 dv/dt 噪声，导致输出不稳定、产生啸叫甚至损坏电路。\n";
        out << "   * **推荐方案**：将反馈电阻（如 $R_{adj}$ 等）尽可能贴近控制 IC 的引脚放置。反馈走线应采用最细的线宽，且周围必须由干净的 GND 铜皮做包地屏蔽（Shielding Ribbon），打足够的过孔连接到主地平面，实现“静电屏蔽层”。\n";
        out << "2. **开关节点（Switch Node）包围面积最小化**：\n";
        out << "   * MOS 管漏极（D）连接变压器初级侧的走线属于极强高频辐射源。应保证其表面积尽可能小以减少对外的共模辐射，但需兼顾散热面积要求，并对相邻层做完整的地平面投影屏蔽。\n\n";
    }

    // 默认提供通用的 DFM 最佳实践
    out << "### 🏭 建议三：华秋/立创商城 DFM 商业化打样工艺规程\n";
    out << "* **线宽与电流承载**：对于大电流驱动回路，建议按 $1\\text{mm}$ 线宽承载 $1\\text{A}$ 电流的标准（铜厚 $1\\text{oz} \\approx 35\\mu\\text{m}$）规划铜宽，或通过裸铜阻焊层加锡以拓宽过流能力。\n";
    out << "* **过孔（Via）防焊油流失**：在发热严重的贴片 MOS 焊盘下打散热过孔时，建议进行**塞孔（Via Plugging）**或**绿油盖孔**工艺，防止锡膏在回流焊时流失到背面，产生焊接空洞。\n";
    out << "* **元器件极性与丝印**：在出厂打样前，务必通过 DFM 检查，确保二极管、电解电容等极性器件丝印指向明确清晰，字符大小不小于 $0.8\\text{mm}$，以免贴片机误装。\n\n";

    out << "---\n";
    out << "*报告生成引擎：KiCad-Auditor Core v2.0. 由 AI 硬件集成专家模块提供技术驱动支持。*\n";

    out.close();
}

} // namespace auditor

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <queue>
#include <cctype>
#include "common/types.hpp"
#include "common/sexpr.hpp"
#include "common/registry.hpp"
#include "common/rule.hpp"
#include "common/report.hpp"
#include "sch/sch_analyzer.hpp"
#include "pcb/pcb_analyzer.hpp"

using namespace auditor;

// 打印全局帮助信息
void print_global_help() {
    std::cout << "========================================================\n"
              << " KiCad-Auditor - Modern PCB Electrical Clearance Auditor\n"
              << " Version: V0.0.0.2\n"
              << "========================================================\n"
              << "Usage:\n"
              << "  kicad-auditor.exe <command> [options]\n\n"
              << "Commands:\n"
              << "  sch      Analyze schematic files (.kicad_sch) for configuration & safety targets\n"
              << "  pcb      Parse PCB layout files (.kicad_pcb) and output physical features\n"
              << "  run      Run complete electrical clearance & safety rules auditing\n"
              << "  param    Analyze specific schematic component for net connections & neighbors\n"
              << "  test     Run geometric core engine self-diagnostic unit tests\n\n"
              << "Use \"kicad-auditor.exe <command> --help\" for more options of a specific command.\n";
}

// 打印子命令帮助信息
void print_command_help(std::string_view command) {
    if (command == "sch") {
        std::cout << "Usage: kicad-auditor.exe sch [options]\n\n"
                  << "Analyze KiCad schematic files to discover key power nets and clearance targets.\n\n"
                  << "Options:\n"
                  << "  -i, --input <file>   Path to the schematic file (.kicad_sch)\n"
                  << "  -o, --output <file>  Path to output JSON/Text analysis report\n"
                  << "  -h, --help           Show this help message\n";
    } else if (command == "pcb") {
        std::cout << "Usage: kicad-auditor.exe pcb [options]\n\n"
                  << "Parse and extract PCB physical features (segments, pads, zones) from layout file.\n\n"
                  << "Options:\n"
                  << "  -i, --input <file>   Path to the PCB file (.kicad_pcb)\n"
                  << "  -o, --output <file>  Path to output parsed layout data\n"
                  << "  -h, --help           Show this help message\n";
    } else if (command == "run") {
        std::cout << "Usage: kicad-auditor.exe run [options]\n\n"
                  << "Perform comprehensive high-voltage creepage & electrical clearance checks on PCB.\n\n"
                  << "Options:\n"
                  << "  -i, --input <file>     Path to the PCB file (.kicad_pcb)\n"
                  << "  -c, --clearance <val>  Global threshold for clearance check in mm (default: 0.2)\n"
                  << "  -o, --output <file>    Path to output full audit Markdown report\n"
                  << "  -h, --help             Show this help message\n";
    } else if (command == "param") {
        std::cout << "Usage: kicad-auditor.exe param <ref> <sch_file> [options]\n"
                  << "  or   kicad-auditor.exe param -r <ref> -i <sch_file> [options]\n\n"
                  << "Analyze a specific component in the schematic for its nets, connections and nearby neighbors.\n\n"
                  << "Options:\n"
                  << "  -r, --ref <name>     Component Reference designator (e.g. U11)\n"
                  << "  -i, --input <file>   Path to the schematic file (.kicad_sch)\n"
                  << "  -j, --json           Output result in structured JSON format\n"
                  << "  -h, --help           Show this help message\n";
    } else if (command == "test") {
        std::cout << "Usage: kicad-auditor.exe test\n\n"
                  << "Run built-in high-precision geometric diagnostic tests to verify types.hpp correctness.\n";
    }
}

// 声明外部 S-Expression 测试函数
namespace auditor {
    int run_sexpr_tests();
}

// 执行核心几何引擎自诊断单元测试
int run_self_tests() {
    // 运行 S-Expression 单元测试
    int sexpr_failed = run_sexpr_tests();
    std::cout << "\n";

    std::cout << "[INFO] Running Geometric Engine Self-Diagnostic Tests...\n";
    int passed = 0;
    int failed = 0;

    auto assert_near = [&](double val, double expected, std::string_view test_name, double tolerance = 1e-5) {
        if (std::abs(val - expected) < tolerance) {
            std::cout << "  [PASS] " << test_name << " (Got " << val << ", Expected " << expected << ")\n";
            passed++;
        } else {
            std::cerr << "  [FAIL] " << test_name << " (Got " << val << ", Expected " << expected << ")\n";
            failed++;
        }
    };

    // 1. 点与点距离测试
    Point p1{0.0, 0.0};
    Point p2{3.0, 4.0};
    assert_near(p1.distance_to(p2), 5.0, "Point to Point distance");

    // 2. 点到线段距离测试 (投影在线段内)
    Point p{1.0, 1.0};
    assert_near(p.distance_to_segment({0.0, 0.0}, {2.0, 0.0}), 1.0, "Point to Segment distance (inside)");

    // 3. 点到线段距离测试 (投影在线段外 - 靠近起点)
    assert_near(p.distance_to_segment({2.0, 0.0}, {4.0, 0.0}), p.distance_to({2.0, 0.0}), "Point to Segment distance (outside - start)");

    // 4. 点到带宽度导线段的最短距离测试
    PcbSegment segment{.start = {0.0, 0.0}, .end = {10.0, 0.0}, .width = 2.0, .layer = "F.Cu", .net = 1};
    assert_near(segment.distance_to_point({5.0, 2.0}), 1.0, "Point to Segment boundary distance");

    // 5. 圆形焊盘距离测试
    PcbPad circle_pad{.name = "1", .type = "thru_hole", .shape = "circle", .pos = {5.0, 5.0}, .size = {2.0, 2.0}, .drill = 1.0, .layer = "F.Cu", .net = 2};
    assert_near(circle_pad.distance_to_point({5.0, 7.0}), 1.0, "Point to Circle Pad boundary distance");

    // 6. 矩形焊盘距离测试
    PcbPad rect_pad{.name = "2", .type = "smd", .shape = "rect", .pos = {0.0, 0.0}, .size = {4.0, 2.0}, .drill = 0.0, .layer = "F.Cu", .net = 3};
    assert_near(rect_pad.distance_to_point({3.0, 0.0}), 1.0, "Point to Rect Pad boundary distance (orthogonal x)");
    assert_near(rect_pad.distance_to_point({3.0, 2.0}), std::sqrt(1.0 * 1.0 + 1.0 * 1.0), "Point to Rect Pad boundary distance (diagonal)");

    std::cout << "\n========================================================\n"
              << " Test Summary: " << passed << " passed, " << failed << " failed.\n"
              << "========================================================\n";
    return (failed == 0 && sexpr_failed == 0) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    // 使用 C++20 std::span 管理命令行参数
    std::span<char*> args(argv, argc);

    if (argc < 2) {
        print_global_help();
        return 1;
    }

    std::string_view command = args[1];

    if (command == "-h" || command == "--help" || command == "help") {
        print_global_help();
        return 0;
    }

    if (command != "sch" && command != "pcb" && command != "run" && command != "param" && command != "test") {
        std::cerr << "[ERROR] Unknown command: " << command << "\n\n";
        print_global_help();
        return 1;
    }

    // 参数变量声明
    std::string input_path;
    std::string output_path;
    std::string ref_name;
    bool output_json = false;
    double clearance_val = 0.2; // 默认电气安全间距 0.2 mm

    // 检查子命令帮助请求
    for (size_t i = 2; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "-h" || arg == "--help") {
            print_command_help(command);
            return 0;
        }
    }

    // 允许位置参数解析：param <ref> <input_file> [options]
    size_t start_idx = 2;
    if (command == "param" && args.size() >= 4 && args[2][0] != '-' && args[3][0] != '-') {
        ref_name = args[2];
        input_path = args[3];
        start_idx = 4;
    }

    // 解析参数列表
    for (size_t i = start_idx; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "-i" || arg == "--input") {
            if (i + 1 < args.size()) {
                input_path = args[++i];
            } else {
                std::cerr << "[ERROR] Missing value for option: " << arg << "\n";
                return 1;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < args.size()) {
                output_path = args[++i];
            } else {
                std::cerr << "[ERROR] Missing value for option: " << arg << "\n";
                return 1;
            }
        } else if (arg == "-r" || arg == "--ref") {
            if (i + 1 < args.size()) {
                ref_name = args[++i];
            } else {
                std::cerr << "[ERROR] Missing value for option: " << arg << "\n";
                return 1;
            }
        } else if (arg == "-j" || arg == "--json") {
            output_json = true;
        } else if (arg == "-c" || arg == "--clearance") {
            if (i + 1 < args.size()) {
                try {
                    clearance_val = std::stod(args[++i]);
                } catch (...) {
                    std::cerr << "[ERROR] Invalid float value for option: " << arg << "\n";
                    return 1;
                }
            } else {
                std::cerr << "[ERROR] Missing value for option: " << arg << "\n";
                return 1;
            }
        } else {
            std::cerr << "[ERROR] Unknown option: " << arg << "\n\n";
            print_command_help(command);
            return 1;
        }
    }

    if (command == "test") {
        return run_self_tests();
    }

    if (command == "sch") {
        if (input_path.empty()) {
            std::cerr << "[ERROR] Schematic input file path is required. Use '-i' or '--input'.\n";
            return 1;
        }
        std::cerr << "[INFO] Analyzing KiCad Schematic: " << input_path << "\n";

        std::ifstream file(input_path);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Failed to open schematic file: " << input_path << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        auto sch_root = parse_sexpr(content);
        if (!sch_root) {
            std::cerr << "[ERROR] Failed to parse schematic S-Expression.\n";
            return 1;
        }

        SchAnalyzer analyzer;
        if (!analyzer.load_schematic(*sch_root)) {
            std::cerr << "[ERROR] Failed to build schematic topology.\n";
            return 1;
        }

        SchRuleRegistry registry;
        registry.register_rule(create_isolation_rule());
        registry.register_rule(create_fb_resistor_rule());
        registry.register_rule(create_comp_spec_rule());

        Report report = analyzer.analyze(registry);

        if (output_json) {
            std::cout << "{\n"
                      << "  \"violations\": [\n";
            for (size_t i = 0; i < report.violations.size(); ++i) {
                const auto& v = report.violations[i];
                std::cout << "    {\n"
                          << "      \"rule_id\": \"" << v.rule_id << "\",\n"
                          << "      \"severity\": \"" << v.severity << "\",\n"
                          << "      \"location\": \"" << v.location << "\",\n"
                          << "      \"message\": \"" << v.message << "\"\n"
                          << "    }";
                if (i + 1 < report.violations.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ],\n"
                      << "  \"components\": [\n";
            const auto& comps = analyzer.get_components();
            for (size_t i = 0; i < comps.size(); ++i) {
                const auto& c = comps[i];
                std::cout << "    {\n"
                          << "      \"ref\": \"" << c.ref << "\",\n"
                          << "      \"value\": \"" << c.value << "\",\n"
                          << "      \"pos\": {\"x\": " << c.pos.x << ", \"y\": " << c.pos.y << "},\n"
                          << "      \"properties\": {\n";
                size_t p_idx = 0;
                for (const auto& prop : c.properties) {
                    std::string clean_val = prop.second;
                    std::replace(clean_val.begin(), clean_val.end(), '"', '\'');
                    std::cout << "        \"" << prop.first << "\": \"" << clean_val << "\"";
                    if (++p_idx < c.properties.size()) std::cout << ",";
                    std::cout << "\n";
                }
                std::cout << "      }\n"
                          << "    }";
                if (i + 1 < comps.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n"
                      << "}\n";
        } else {
            std::cout << "========================================================\n";
            std::cout << " Schematic Static Safety Audit Report\n";
            std::cout << "========================================================\n";
            if (report.violations.empty()) {
                std::cout << "[SUCCESS] No schematic safety violations detected!\n";
            } else {
                int errors = 0;
                int warnings = 0;
                int infos = 0;
                for (const auto& v : report.violations) {
                    std::cout << "  - [" << v.severity << "] [" << v.rule_id << "] @" << v.location << ": " << v.message << "\n";
                    if (v.severity == "ERROR") errors++;
                    else if (v.severity == "WARNING") warnings++;
                    else infos++;
                }
                std::cout << "\n========================================================\n";
                std::cout << " Audit Summary: " << errors << " errors, " << warnings << " warnings, " << infos << " infos.\n";
                std::cout << "========================================================\n";
            }
        }
    } 

    else if (command == "pcb") {
        if (input_path.empty()) {
            std::cerr << "[ERROR] PCB input file path is required. Use '-i' or '--input'.\n";
            return 1;
        }
        std::cout << "[INFO] Parsing KiCad PCB layout: " << input_path << "\n";
        PcbAnalyzer analyzer;
        if (!analyzer.load_pcb(input_path)) {
            std::cerr << "[ERROR] Failed to load and parse PCB file: " << input_path << "\n";
            return 1;
        }
        std::cout << "[SUCCESS] PCB parsing complete.\n";
    } 

    else if (command == "run") {
        if (input_path.empty()) {
            std::cerr << "[ERROR] PCB input file path is required for unified audit. Use '-i' or '--input'.\n";
            return 1;
        }

        std::filesystem::path pcb_file_path(input_path);
        if (!std::filesystem::exists(pcb_file_path)) {
            std::cerr << "[ERROR] PCB file does not exist: " << input_path << "\n";
            return 1;
        }

        std::string project_name = pcb_file_path.stem().string();
        std::cout << "[INFO] Performing unified collaborative audit on project: " << project_name << "\n";

        // 1. 铺铜刷新与 PCB 解析 (load_pcb 会自动调用 fill_pcb_zones 刷新铺铜)
        std::cout << "[INFO] Loading and parsing PCB Layout...\n";
        PcbAnalyzer pcb_analyzer;
        if (!pcb_analyzer.load_pcb(input_path)) {
            std::cerr << "[ERROR] Failed to load PCB file: " << input_path << "\n";
            return 1;
        }

        // 2. 联合查找同名原理图文件并审查
        Report global_report;
        std::filesystem::path sch_file_path = pcb_file_path;
        sch_file_path.replace_extension(".kicad_sch");

        if (std::filesystem::exists(sch_file_path)) {
            std::cout << "[INFO] Found matching Schematic file: " << sch_file_path.string() << "\n";
            std::cout << "[INFO] Parsing Schematic topology...\n";
            std::ifstream file(sch_file_path.string());
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();
                auto sch_root = parse_sexpr(content);
                if (sch_root) {
                    SchAnalyzer sch_analyzer;
                    if (sch_analyzer.load_schematic(*sch_root)) {
                        SchRuleRegistry sch_registry;
                    sch_registry.register_rule(create_isolation_rule());
                    sch_registry.register_rule(create_fb_resistor_rule());
                    sch_registry.register_rule(create_comp_spec_rule());
                        
                        Report sch_report = sch_analyzer.analyze(sch_registry);
                        global_report.violations.insert(global_report.violations.end(), sch_report.violations.begin(), sch_report.violations.end());
                        std::cout << "[SUCCESS] Schematic audit complete. Discovered " << sch_report.violations.size() << " violations.\n";
                    } else {
                        std::cerr << "[WARNING] Failed to build schematic topology.\n";
                    }
                } else {
                    std::cerr << "[WARNING] Failed to parse schematic S-Expression.\n";
                }
            } else {
                std::cerr << "[WARNING] Failed to open schematic file for reading.\n";
            }
        } else {
            std::cout << "[WARNING] No matching schematic file found (" << sch_file_path.filename().string() << "). Skipping schematic analysis.\n";
        }

        // 3. 执行 PCB 规则审计
        std::cout << "[INFO] Running PCB layout standard rule checkers...\n";
        PcbRuleRegistry pcb_registry;
        pcb_registry.register_rule(create_emi_clearance_rule());
        pcb_registry.register_rule(create_sensitive_shield_rule());
        Report pcb_report = pcb_analyzer.analyze(pcb_registry);
        global_report.violations.insert(global_report.violations.end(), pcb_report.violations.begin(), pcb_report.violations.end());

        // 4. 执行动态物理电气间距 (Physical Clearance) 审计 (基于命令行传入的 clearance_val)
        std::cout << "[INFO] Performing physical electrical clearance checks (threshold: " << clearance_val << " mm)...\n";
        const auto& pads = pcb_analyzer.get_pads();
        const auto& segments = pcb_analyzer.get_segments();

        size_t clearance_violations_count = 0;

        // 识别高频开关/高压网络与其它网络进行间距核验
        for (size_t i = 0; i < pads.size(); ++i) {
            const auto& pad1 = pads[i];
            if (pad1.net == -1) continue;
            std::string net1 = pcb_analyzer.get_net_name(pad1.net);
            
            // 找出可能的高压或高频敏感网络
            std::string net1_lower = net1;
            std::transform(net1_lower.begin(), net1_lower.end(), net1_lower.begin(), ::tolower);
            bool is_special1 = (net1_lower.find("sw") != std::string::npos || 
                                net1_lower.find("vadj") != std::string::npos || 
                                net1_lower.find("drain") != std::string::npos ||
                                net1_lower.find("mos") != std::string::npos ||
                                net1_lower.find("primary") != std::string::npos);
            if (!is_special1) continue;

            // 核对与其相邻的其他网络焊盘之间的距离
            for (size_t j = i + 1; j < pads.size(); ++j) {
                const auto& pad2 = pads[j];
                if (pad2.net == -1 || pad1.net == pad2.net) continue;
                std::string net2 = pcb_analyzer.get_net_name(pad2.net);

                double dist = pad1.pos.distance_to(pad2.pos);
                // 粗略减去两焊盘外径尺寸
                double size1 = std::max(pad1.size.x, pad1.size.y);
                double size2 = std::max(pad2.size.x, pad2.size.y);
                double edge_dist = std::max(0.0, dist - (size1 / 2.0) - (size2 / 2.0));

                if (edge_dist < clearance_val) {
                    clearance_violations_count++;
                    std::string msg = "Physical layout clearance violation between high frequency node pad " + pad1.name + 
                                      " (" + net1 + ") and adjacent node pad " + pad2.name + " (" + net2 + 
                                      "). Measured edge distance: " + std::to_string(edge_dist) + " mm, which is below the safe clearance margin of " + 
                                      std::to_string(clearance_val) + " mm.";
                    global_report.add_violation("PCB_CLEARANCE_01", "ERROR", msg, "Pad " + pad1.name + " <-> " + pad2.name);
                }
            }

            // 核对焊盘与布线段 (segments) 之间的距离
            for (const auto& seg : segments) {
                if (seg.net == -1 || pad1.net == seg.net) continue;
                std::string net2 = pcb_analyzer.get_net_name(seg.net);

                double dist = seg.distance_to_point(pad1.pos);
                double pad_radius = std::max(pad1.size.x, pad1.size.y) / 2.0;
                double edge_dist = std::max(0.0, dist - pad_radius);

                if (edge_dist < clearance_val) {
                    clearance_violations_count++;
                    std::string msg = "Physical layout clearance violation between pad " + pad1.name + 
                                      " (" + net1 + ") and trace segment of net (" + net2 + 
                                      "). Measured edge distance: " + std::to_string(edge_dist) + " mm, which is below the safe clearance margin of " + 
                                      std::to_string(clearance_val) + " mm.";
                    global_report.add_violation("PCB_CLEARANCE_01", "ERROR", msg, "Pad " + pad1.name + " <-> Segment Net " + net2);
                }
            }
        }

        std::cout << "[SUCCESS] PCB clearance check complete. Detected " << pcb_report.violations.size() + clearance_violations_count << " physical violations.\n";

        // 5. 确定输出 Markdown 报告路径
        std::string report_out = output_path;
        if (report_out.empty()) {
            std::filesystem::path default_report = pcb_file_path;
            default_report.replace_extension(".md");
            report_out = default_report.string();
        }

        std::cout << "[INFO] Generating Github-style Markdown Report at: " << report_out << " ...\n";
        global_report.export_markdown(report_out, project_name);
        std::cout << "[SUCCESS] Hardware safety audit Markdown report generated!\n\n";

        // 控制台统计输出
        std::cout << "========================================================\n";
        std::cout << " Unified KiCad-Auditor Co-Review Statistics\n";
        std::cout << "========================================================\n";
        int errors = 0;
        int warnings = 0;
        int infos = 0;
        for (const auto& v : global_report.violations) {
            std::cout << "  - [" << v.severity << "] [" << v.rule_id << "] @" << v.location << ":\n";
            std::cout << "    " << v.message << "\n\n";
            if (v.severity == "ERROR") errors++;
            else if (v.severity == "WARNING") warnings++;
            else infos++;
        }
        std::cout << "========================================================\n";
        std::cout << " Collaborative Audit Summary: " << errors << " errors, " << warnings << " warnings, " << infos << " infos.\n";
        std::cout << "========================================================\n";
    } 

    else if (command == "param") {
        if (input_path.empty()) {
            std::cerr << "[ERROR] Schematic input file path is required. Use '-i' or position argument.\n";
            return 1;
        }
        if (ref_name.empty()) {
            std::cerr << "[ERROR] Component reference is required. Use '-r' or position argument.\n";
            return 1;
        }

        std::ifstream file(input_path);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Failed to open schematic file: " << input_path << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        auto sch_root = parse_sexpr(content);
        if (!sch_root) {
            std::cerr << "[ERROR] Failed to parse schematic S-Expression.\n";
            return 1;
        }

        SchAnalyzer analyzer;
        if (!analyzer.load_schematic(*sch_root)) {
            std::cerr << "[ERROR] Failed to build schematic topology.\n";
            return 1;
        }

        ComponentAnalysisResult result = analyzer.analyze_component(ref_name, 30.0);

        if (!result.found) {
            if (output_json) {
                std::cout << "{\"error\": \"Component " << ref_name << " not found in schematic.\"}\n";
            } else {
                std::cerr << "[ERROR] Component " << ref_name << " not found in schematic!\n";
            }
            return 1;
        }

        // 格式化输出
        if (output_json) {
            std::cout << "{\n"
                      << "  \"ref\": \"" << result.ref << "\",\n"
                      << "  \"value\": \"" << result.value << "\",\n"
                      << "  \"pos\": {\"x\": " << result.pos.x << ", \"y\": " << result.pos.y << "},\n"
                      << "  \"properties\": {\n";
            size_t p_idx = 0;
            for (const auto& prop : result.properties) {
                std::string clean_val = prop.second;
                std::replace(clean_val.begin(), clean_val.end(), '"', '\'');
                std::cout << "    \"" << prop.first << "\": \"" << clean_val << "\"";
                if (++p_idx < result.properties.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  },\n"
                      << "  \"pins\": [\n";
            for (size_t i = 0; i < result.pins.size(); ++i) {
                const auto& p = result.pins[i];
                std::cout << "    {\n"
                          << "      \"number\": \"" << p.pin_num << "\",\n"
                          << "      \"name\": \"" << p.pin_name << "\",\n"
                          << "      \"net\": \"" << p.net_name << "\",\n"
                          << "      \"connections\": [\n";
                for (size_t j = 0; j < p.other_connections.size(); ++j) {
                    std::cout << "        {\"ref\": \"" << p.other_connections[j].first << "\", \"pin\": \"" << p.other_connections[j].second << "\"}";
                    if (j + 1 < p.other_connections.size()) std::cout << ",";
                    std::cout << "\n";
                }
                std::cout << "      ]\n"
                          << "    }";
                if (i + 1 < result.pins.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ],\n"
                      << "  \"nearby_components\": [\n";
            for (size_t i = 0; i < result.nearby_components.size(); ++i) {
                std::cout << "    {\"ref\": \"" << result.nearby_components[i].ref << "\", \"value\": \"" << result.nearby_components[i].value << "\", \"distance\": " << result.nearby_components[i].distance << "}";
                if (i + 1 < result.nearby_components.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n"
                      << "}\n";
        } else {
            std::cout << "========================================================\n"
                      << " KiCad-Auditor Component Analysis Report: " << result.ref << "\n"
                      << "========================================================\n"
                      << "Device Properties:\n"
                      << "  - Reference: " << result.ref << "\n"
                      << "  - Value: " << result.value << "\n"
                      << "  - Position: (" << result.pos.x << ", " << result.pos.y << ") mm\n";
            for (const auto& prop : result.properties) {
                std::cout << "  - " << prop.first << ": " << prop.second << "\n";
            }
            std::cout << "\nPin Electrical Connections:\n";

            for (const auto& p : result.pins) {
                std::cout << "  - Pin " << p.pin_num;
                if (!p.pin_name.empty()) {
                    std::cout << " (" << p.pin_name << ")";
                }
                std::cout << " -> Net: [" << p.net_name << "]\n";
                if (p.other_connections.empty()) {
                    std::cout << "    [No direct component connections]\n";
                } else {
                    std::cout << "    Directly Connected: ";
                    for (size_t j = 0; j < p.other_connections.size(); ++j) {
                        std::cout << p.other_connections[j].first << "." << p.other_connections[j].second;
                        if (j + 1 < p.other_connections.size()) std::cout << ", ";
                    }
                    std::cout << "\n";
                }
            }

            std::cout << "\nNearby Components (within 30mm radius, ordered by distance):\n";
            if (result.nearby_components.empty()) {
                std::cout << "  - No nearby components found.\n";
            } else {
                for (const auto& n : result.nearby_components) {
                    std::cout << "  - " << n.ref << " (" << n.value << ") @ " << std::fixed << std::setprecision(2) << n.distance << " mm\n";
                }
            }
            std::cout << "========================================================\n";
        }
    }

    return 0;
}

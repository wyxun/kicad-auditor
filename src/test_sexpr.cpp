#include "common/sexpr.hpp"
#include "common/rule.hpp"
#include "common/registry.hpp"
#include "common/json.hpp"
#include "sch/sch_analyzer.hpp"
#include "pcb/pcb_analyzer.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>

namespace auditor {

// Mock rules for Strategy Pattern testing
class MockSchRule : public SchRule {
public:
    std::string get_id() const override { return "SCH_MOCK_01"; }
    std::string get_description() const override { return "Mock schematic check rule"; }
    void analyze(const SExpr& /*sch*/, Report& report) override {
        report.add_violation(get_id(), "WARNING", "Mock schematic warning", "U1");
    }
};

class MockPcbRule : public PcbRule {
public:
    std::string get_id() const override { return "PCB_MOCK_01"; }
    std::string get_description() const override { return "Mock PCB check rule"; }
    void analyze(const SExpr& /*pcb*/, Report& report) override {
        report.add_violation(get_id(), "ERROR", "Mock PCB error", "Pad 1 of Q1");
    }
};

int run_sexpr_tests() {
    std::cout << "[INFO] Running S-Expression Parser Self-Diagnostic Tests...\n";
    int passed = 0;
    int failed = 0;

    auto assert_true = [&](bool condition, std::string_view test_name) {
        if (condition) {
            std::cout << "  [PASS] " << test_name << "\n";
            passed++;
        } else {
            std::cerr << "  [FAIL] " << test_name << "\n";
            failed++;
        }
    };

    auto assert_near = [&](double val, double expected, std::string_view test_name, double tolerance = 1e-5) {
        if (std::abs(val - expected) < tolerance) {
            std::cout << "  [PASS] " << test_name << " (Got " << val << ", Expected " << expected << ")\n";
            passed++;
        } else {
            std::cerr << "  [FAIL] " << test_name << " (Got " << val << ", Expected " << expected << ")\n";
            failed++;
        }
    };

    // 1. 基础 S-Expression 解析测试 (关键字与叶子节点)
    {
        std::string_view input = "(symbol \"TRAN_UL\" (at 159.2 39.8))";
        auto root = parse_sexpr(input);
        
        assert_true(root != nullptr, "Parse non-null root");
        if (root) {
            assert_true(root->head == "symbol", "Root head matches 'symbol'");
            assert_true(root->children.size() == 2, "Root child count is 2");
            
            // 第一个子节点是 "TRAN_UL" 叶子节点
            assert_true(root->children[0]->is_leaf(), "First child is a leaf node");
            assert_true(root->children[0]->value == "TRAN_UL", "First child value matches");
            
            // 第二个子节点是 (at 159.2 39.8) 子表达式
            const auto* at_node = root->find_child("at");
            assert_true(at_node != nullptr, "Find child 'at' successfully");
            if (at_node) {
                assert_true(at_node->head == "at", "Child head matches 'at'");
                assert_near(at_node->get_double_at(0), 159.2, "Get double x from 'at'");
                assert_near(at_node->get_double_at(1), 39.8, "Get double y from 'at'");
            }
        }
    }

    // 2. 带转义字符的双引号字符串解析测试
    {
        std::string_view input = "(pin \"1\" \"GND\\nPower\" (at 10 20))";
        auto root = parse_sexpr(input);
        assert_true(root != nullptr, "Parse escape characters");
        if (root) {
            assert_true(root->head == "pin", "Head matches 'pin'");
            assert_true(root->get_value_at(0) == "1", "First parameter is '1'");
            assert_true(root->get_value_at(1) == "GND\nPower", "Second parameter correctly resolves escape '\\n'");
        }
    }

    // 3. 空白字符与异常格式容错性测试
    {
        std::string_view input = "  (   pad   \"1\"   smd   circle (size   2.5 \t 2.5 )  )  ";
        auto root = parse_sexpr(input);
        assert_true(root != nullptr, "Parse whitespace tolerant");
        if (root) {
            assert_true(root->head == "pad", "Head matches 'pad'");
            assert_true(root->get_value_at(0) == "1", "First parameter is '1'");
            assert_true(root->get_value_at(1) == "smd", "Second parameter is 'smd'");
            assert_true(root->get_value_at(2) == "circle", "Third parameter is 'circle'");
            
            const auto* size_node = root->find_child("size");
            assert_true(size_node != nullptr, "Find nested 'size'");
            if (size_node) {
                assert_near(size_node->get_double_at(0), 2.5, "Size width");
                assert_near(size_node->get_double_at(1), 2.5, "Size height");
            }
        }
    }

    // 4. 深拷贝完整性测试
    {
        std::string_view input = "(zone (net 2) (layer \"F.Cu\") (outline (v 0 0) (v 10 0) (v 10 10)))";
        auto root = parse_sexpr(input);
        assert_true(root != nullptr, "Parse multi-level zone");
        if (root) {
            // 进行深拷贝
            SExpr root_copy = *root;
            
            // 改变原 root，验证深拷贝的独立性
            root->head = "zone_modified";
            root->children.clear();
            
            assert_true(root_copy.head == "zone", "Deep copy preserves head");
            assert_true(root_copy.children.size() == 3, "Deep copy preserves children count");
            
            const auto* net_node = root_copy.find_child("net");
            assert_true(net_node != nullptr, "Deep copy child query works");
            if (net_node) {
                assert_true(net_node->get_int_at(0) == 2, "Deep copy values are preserved");
            }
        }
    }

    // 5. 极端边界测试 (空括号, 无括号, 负数和混合)
    {
        auto root = parse_sexpr("()");
        assert_true(root != nullptr && root->head.empty() && root->children.empty(), "Empty brackets parse successfully");

        auto atom_root = parse_sexpr("atom_only");
        assert_true(atom_root != nullptr && atom_root->value == "atom_only" && atom_root->is_leaf(), "Single atom outside brackets");
    }
    // 6. Task 3: 策略注册与规则管理器测试
    {
        SchRuleRegistry sch_registry;
        PcbRuleRegistry pcb_registry;

        sch_registry.register_rule(std::make_unique<MockSchRule>());
        pcb_registry.register_rule(std::make_unique<MockPcbRule>());

        assert_true(sch_registry.get_rules().size() == 1, "Sch rule registered successfully");
        assert_true(pcb_registry.get_rules().size() == 1, "Pcb rule registered successfully");

        SExpr empty_sexpr;

        Report sch_report = sch_registry.run_all(empty_sexpr);
        Report pcb_report = pcb_registry.run_all(empty_sexpr);

        assert_true(sch_report.violations.size() == 1, "Sch rule triggered");
        if (!sch_report.violations.empty()) {
            assert_true(sch_report.violations[0].rule_id == "SCH_MOCK_01", "Sch violation rule ID matches");
            assert_true(sch_report.violations[0].severity == "WARNING", "Sch violation severity matches");
        }

        assert_true(pcb_report.violations.size() == 1, "Pcb rule triggered");
        assert_true(pcb_report.has_errors(), "Pcb report correctly indicates it has errors");
    }

    // 7. Task 3: 零依赖微型 JSON 解析器测试
    {
        auto empty_obj = parse_json("{}");
        assert_true(empty_obj.is_object() && empty_obj.obj_val.empty(), "Parse empty JSON object");

        auto empty_arr = parse_json("[]");
        assert_true(empty_arr.is_array() && empty_arr.arr_val.empty(), "Parse empty JSON array");

        std::string_view complex_json = R"({
            "name": "KiCad Auditor",
            "version": 3.0,
            "active": true,
            "tags": ["pcb", "hardware"],
            "meta": {
                "author": "Antigravity"
            }
        })";
        auto root = parse_json(complex_json);
        assert_true(root.is_object(), "Parse complex JSON root");
        assert_true(root["name"].is_string() && root["name"].str_val == "KiCad Auditor", "Parse JSON String");
        assert_near(root["version"].num_val, 3.0, "Parse JSON Number");
        assert_true(root["active"].is_bool() && root["active"].bool_val == true, "Parse JSON Boolean");

        assert_true(root["tags"].is_array() && root["tags"].arr_val.size() == 2, "Parse JSON Array");
        assert_true(root["tags"][0].str_val == "pcb", "Index array item");

        assert_true(root["meta"].is_object(), "Parse nested object");
        assert_true(root["meta"]["author"].str_val == "Antigravity", "Access nested object key");
    }

    // 8. Task 3: 本地元器件规则数据库读取测试
    {
        std::ifstream file("data/components_db.json");
        assert_true(file.is_open(), "Locate and open data/components_db.json");

        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            auto root = parse_json(content);
            assert_true(root.is_object(), "Database root is an object");
            
            auto comps = root["components"];
            assert_true(comps.is_array(), "Database has components array");
            assert_true(comps.arr_val.size() >= 3, "Database has at least 3 components registered");

            bool found_mos = false;
            for (const auto& item : comps.arr_val) {
                if (item["lcsc_id"].str_val == "C283625") {
                    found_mos = true;
                    assert_true(item["name"].str_val.find("MOSFET") != std::string::npos, "C283625 name contains MOSFET");
                    assert_near(item["max_voltage"].num_val, 100.0, "C283625 max voltage is 100V");
                    assert_near(item["max_current"].num_val, 50.0, "C283625 max current is 50A");
                }
            }
            assert_true(found_mos, "Database contains LCSC ID C283625");
        }
    }

    // 9. Task 4: 原理图安全诊断引擎与图论拓扑算法测试
    {
        std::string_view sch_mock_content = R"(
(schematic
  ;; 1. 正常的光耦隔离器件 U1
  (symbol (property "Reference" "U1") (property "Value" "PC817") (property "Footprint" "Optocoupler_DIP-4") (property "LCSC" (value "C12345")) (at 100 100)
    (pin "1" (at -10 -10))  ; 绝对坐标 (90, 90) -> 初级输入+
    (pin "2" (at -10 10))   ; 绝对坐标 (90, 110) -> 初级地 (GND)
    (pin "3" (at 10 10))    ; 绝对坐标 (110, 110) -> 次级地 (SGND)
    (pin "4" (at 10 -10))   ; 绝对坐标 (110, 90) -> 次级输出
  )

  ;; 2. 异常的光耦隔离器件 U2 (故意在外面把初次级 GND 连通)
  (symbol (property "Reference" "U2") (property "Value" "PC817") (at 200 100)
    (pin "1" (at -10 -10))  ; 绝对坐标 (190, 90)
    (pin "2" (at -10 10))   ; 绝对坐标 (190, 110) -> 初级地
    (pin "3" (at 10 10))    ; 绝对坐标 (210, 110) -> 次级地
    (pin "4" (at 10 -10))   ; 绝对坐标 (210, 90)
  )

  ;; 3. 电源芯片 U3 (关联数据库已有的 C155484, max_voltage 40V, max_current 5A)
  (symbol (property "Reference" "U3") (property "Value" "LM2596") (property "LCSC" (value "C155484")) (at 300 200)
    (pin "FB" (at -10 0))   ; 绝对坐标 (290, 200)
    (pin "EN" (at 10 0))    ; 绝对坐标 (310, 200)
    (pin "SW" (at 0 10))    ; 绝对坐标 (300, 210)
    (pin "VIN" (at 10 10))  ; 绝对坐标 (310, 210)
  )

  ;; 4. FB分压电阻 R1 (1.2M)
  (symbol (property "Reference" "R1") (property "Value" "1.2M") (at 290 150)
    (pin "1" (at 0 -10))    ; 绝对坐标 (290, 140) -> 连 Vout
    (pin "2" (at 0 10))     ; 绝对坐标 (290, 160) -> 连 FB
  )

  ;; 5. FB分压电阻 R2 (200k)
  (symbol (property "Reference" "R2") (property "Value" "200k") (at 290 250)
    (pin "1" (at 0 -10))    ; 绝对坐标 (290, 240) -> 连 FB
    (pin "2" (at 0 10))     ; 绝对坐标 (290, 260) -> 连 SGND
  )

  ;; 6. 故意超载发热的 0603 电阻 R3 (功耗 = 12^2/100 = 1.44W > 0.1W limit)
  (symbol (property "Reference" "R3") (property "Value" "100") (property "Footprint" "Resistor_SMD:R_0603_1608Metric") (at 400 100)
    (pin "1" (at 0 -10))  ; 绝对坐标 (400, 90)
    (pin "2" (at 0 10))   ; 绝对坐标 (400, 110)
  )

  ;; 7. 缺失数据库规格记录的芯片 U4
  (symbol (property "Reference" "U4") (property "Value" "MockChip") (property "LCSC" (value "C999999")) (at 500 200)
    (pin "VCC" (at -10 0)) ; 绝对坐标 (490, 200)
  )

  ;; 8. 电感 L1 (载流 3A < 芯片源 5A)
  (symbol (property "Reference" "L1") (property "Value" "2.2uH") (property "LCSC" (value "C_L_MOCK_3A")) (at 300 300)
    (pin "1" (at 0 -10)) ; 绝对坐标 (300, 290)
    (pin "2" (at 0 10))  ; 绝对坐标 (300, 310)
  )

  ;; 9. 续流二极管 D1 (载流 3A < 芯片源 5A)
  (symbol (property "Reference" "D1") (property "Value" "SS34") (property "LCSC" (value "C_D_MOCK_3A")) (at 200 300)
    (pin "1" (at 0 -10)) ; 绝对坐标 (200, 290)
    (pin "2" (at 0 10))  ; 绝对坐标 (200, 310)
  )

  ;; 10. 导线与网络标签
  (wire (pts (xy 90 110) (xy 80 110)))
  (label "GND" (at 80 110))
  (wire (pts (xy 110 110) (xy 120 110)))
  (label "SGND" (at 120 110))

  ;; 异常的物理短路 (把 190,110 (GND) 和 210,110 (SGND) 通过一根线直接连通了！)
  (wire (pts (xy 190 110) (xy 200 110)))
  (wire (pts (xy 200 110) (xy 210 110)))
  (label "GND" (at 190 110))
  (label "SGND" (at 210 110))

  ;; U3 FB 正常的连接
  (wire (pts (xy 290 200) (xy 290 160))) ; FB 连 R1-2
  (wire (pts (xy 290 200) (xy 290 240))) ; FB 连 R2-1
  (wire (pts (xy 290 260) (xy 290 270))) ; R2-2 连 SGND
  (label "SGND" (at 290 270))
  (wire (pts (xy 290 140) (xy 290 130))) ; R1-1 连 Vout
  (label "Vout" (at 290 130))

  ;; 连线构成开关回路与高压/功耗测试
  (wire (pts (xy 300 210) (xy 300 290))) ; U3.SW 连 L1.1
  (wire (pts (xy 300 290) (xy 200 290))) ; L1.1 连 D1.1 (SW_OUT 网络)
  (label "SW_OUT" (at 300 250))

  (wire (pts (xy 300 310) (xy 300 320))) ; L1.2 连 Vout
  (wire (pts (xy 300 320) (xy 290 130)))
  
  (wire (pts (xy 200 310) (xy 200 320))) ; D1.2 连 GND
  (label "GND" (at 200 320))

  (wire (pts (xy 310 210) (xy 330 210))) ; U3.VIN 连 +50V (触发耐压超载 ERROR)
  (label "+50V" (at 330 210))

  (wire (pts (xy 400 90) (xy 400 80)))   ; R3.1 连 +12V
  (label "+12V" (at 400 80))
  (wire (pts (xy 400 110) (xy 400 120))) ; R3.2 连 GND
  (label "GND" (at 400 120))
)
        )";

        auto sch_root = parse_sexpr(sch_mock_content);
        assert_true(sch_root != nullptr, "Parse Mock Schematic S-Expression successfully");

        if (sch_root) {
            SchAnalyzer analyzer;
            bool load_ok = analyzer.load_schematic(*sch_root);
            assert_true(load_ok, "Build schematic graph topology and component map");

            // 验证器件提取数量
            assert_true(analyzer.get_components().size() == 9, "Extracted exactly 9 schematic components");
            
            // 验证引脚提取
            const auto* u1 = &analyzer.get_components()[0];
            assert_true(u1->ref == "U1", "First component is U1");
            assert_true(u1->pins.size() == 4, "U1 has 4 pins");

            // 验证电气网络标签解析
            std::string net_90_110 = analyzer.get_net_name("90.000,110.000");
            std::string net_110_110 = analyzer.get_net_name("110.000,110.000");
            assert_true(net_90_110 == "GND", "U1 pin 2 is in GND network");
            assert_true(net_110_110 == "SGND", "U1 pin 3 is in SGND network");

            // 验证自定义属性提取
            assert_true(u1->properties.at("Reference") == "U1", "U1 Reference property matches");
            assert_true(u1->properties.at("Value") == "PC817", "U1 Value property matches");
            assert_true(u1->properties.at("Footprint") == "Optocoupler_DIP-4", "U1 Footprint property matches");
            assert_true(u1->properties.at("LCSC") == "C12345", "U1 LCSC property matches (compatible style)");

            // 验证 ComponentAnalysisResult 中自定义属性的传递
            ComponentAnalysisResult analysis_res = analyzer.analyze_component("U1");
            assert_true(analysis_res.found, "U1 component analysis result found");
            assert_true(analysis_res.properties.at("Reference") == "U1", "Analysis result U1 Reference property matches");
            assert_true(analysis_res.properties.at("Value") == "PC817", "Analysis result U1 Value property matches");
            assert_true(analysis_res.properties.at("Footprint") == "Optocoupler_DIP-4", "Analysis result U1 Footprint property matches");
            assert_true(analysis_res.properties.at("LCSC") == "C12345", "Analysis result U1 LCSC property matches (compatible style)");

            // 运行规则库分析
            SchRuleRegistry registry;
            registry.register_rule(create_isolation_rule());
            registry.register_rule(create_fb_resistor_rule());
            registry.register_rule(create_comp_spec_rule());

            Report report = analyzer.analyze(registry);

            std::cout << "[DEBUG] Violations reported:\n";
            for (const auto& v : report.violations) {
                std::cout << "  - [" << v.severity << "] [" << v.rule_id << "] @" << v.location << ": " << v.message << "\n";
            }

            // 验证分析结果
            bool has_u2_iso_error = false;
            bool has_u1_iso_error = false;
            bool has_u3_fb_warning = false;
            bool has_u3_cff_info = false;
            bool has_u3_en_warning = false;

            bool has_u4_spec_info = false;
            bool has_u3_volt_error = false;
            bool has_r3_power_error = false;
            bool has_l1_current_error = false;
            bool has_d1_current_error = false;
            bool has_u3_fb_impedance_warning = false;

            for (const auto& v : report.violations) {
                if (v.rule_id == "SCH_ISO_01") {
                    if (v.location == "U2") has_u2_iso_error = true;
                    if (v.location == "U1") has_u1_iso_error = true;
                } else if (v.rule_id == "SCH_FB_01") {
                    if (v.location == "U3") {
                        if (v.message.find("impedance") != std::string::npos && v.severity == "WARNING") {
                            has_u3_fb_warning = true;
                        }
                        if (v.message.find("feedforward capacitor") != std::string::npos && v.severity == "INFO") {
                            has_u3_cff_info = true;
                        }
                        if (v.message.find("EN") != std::string::npos && v.severity == "WARNING") {
                            has_u3_en_warning = true;
                        }
                    }
                } else if (v.rule_id == "SCH_COMP_01") {
                    if (v.location == "U4" && v.severity == "INFO" && v.message.find("not found") != std::string::npos) {
                        has_u4_spec_info = true;
                    }
                    if (v.location == "U3" && v.severity == "ERROR" && v.message.find("maximum voltage") != std::string::npos) {
                        has_u3_volt_error = true;
                    }
                    if (v.location == "R3" && v.severity == "ERROR" && v.message.find("calculated power") != std::string::npos) {
                        has_r3_power_error = true;
                    }
                    if (v.location == "L1" && v.severity == "ERROR" && v.message.find("saturation current") != std::string::npos) {
                        has_l1_current_error = true;
                    }
                    if (v.location == "D1" && v.severity == "ERROR" && v.message.find("maximum current") != std::string::npos) {
                        has_d1_current_error = true;
                    }
                    if (v.location == "U3" && v.severity == "WARNING" && v.message.find("equivalent impedance") != std::string::npos) {
                        has_u3_fb_impedance_warning = true;
                    }
                }
            }

            assert_true(!has_u1_iso_error, "Assert U1 (correctly isolated) has NO isolation error");
            assert_true(has_u2_iso_error, "Assert U2 (intentionally shorted) has isolation failure ERROR");
            assert_true(has_u3_fb_warning, "Assert U3 has high impedance feedback WARNING (1.21 MOhm)");
            assert_true(has_u3_cff_info, "Assert U3 reports Cff feedforward capacitor missing INFO");
            assert_true(has_u3_en_warning, "Assert U3 has floating EN pin WARNING");

            assert_true(has_u4_spec_info, "Assert U4 reports database spec missing INFO");
            assert_true(has_u3_volt_error, "Assert U3 reports maximum voltage overlimit ERROR (50V > 40V)");
            assert_true(has_r3_power_error, "Assert R3 reports package power dissipation overlimit ERROR");
            assert_true(has_l1_current_error, "Assert L1 reports saturation current safety risk ERROR");
            assert_true(has_d1_current_error, "Assert D1 reports maximum current safety risk ERROR");
            assert_true(has_u3_fb_impedance_warning, "Assert U3 reports equivalent FB impedance WARNING");
        }
    }

    // 10. Task 5: PCB 几何与电磁分析集成测试 (Mock PCB)
    {
        std::string_view pcb_mock_content = R"(
(kicad_pcb
  ;; 网络定义
  (net 0 "")
  (net 1 "SW")
  (net 2 "GND")
  (net 3 "FB")

  ;; 1. 高频开关走线段 SW, 位于 F.Cu, 宽度 0.5mm
  (segment (start 10 10) (end 15 10) (width 0.5) (layer "F.Cu") (net 1))

  ;; 2. 敏感信号走线段 FB, 同样位于 F.Cu, 宽度 0.2mm
  ;; 与 SW 走线靠得非常近 (中心距 0.5mm，边缘间距仅 0.15mm)
  (segment (start 10 10.5) (end 15 10.5) (width 0.2) (layer "F.Cu") (net 3))

  ;; 3. 敏感信号另一段走线段 FB, 位于 F.Cu
  ;; 我们故意不给它在 B.Cu 下面铺地，测试其跨层投影参考地缺口
  (segment (start 20 20) (end 30 20) (width 0.2) (layer "F.Cu") (net 3))

  ;; 4. 大面积 GND 铺铜区 (Zone)，位于 B.Cu 屏蔽层
  ;; 其 outline 多边形顶点故意包含了一个离 SW 走线极近的顶点 (10, 10.25) 
  ;; 该顶点到 SW 走线的 Clearance 极小，估算耦合阻抗将会极低，直接触发 [PCB-01]
  (zone (net 2) (net_name "GND") (layer "B.Cu")
    (filled_polygon
      (pts
        (xy 0 0)
        (xy 100 0)
        (xy 100 15)
        (xy 10 10.25) ; 离 SW (10,10)->(15,10) 仅 0.25mm-0.25mm = 0mm 的超近顶点
        (xy 0 15)
      )
    )
  )
)
        )";

        auto pcb_root = parse_sexpr(pcb_mock_content);
        assert_true(pcb_root != nullptr, "Parse Mock PCB S-Expression successfully");

        if (pcb_root) {
            PcbAnalyzer analyzer;
            analyzer.load_mock_pcb(*pcb_root);

            // 验证实体提取数量
            assert_true(analyzer.get_segments().size() == 3, "Extracted exactly 3 PCB segments");
            assert_true(analyzer.get_zones().size() == 1, "Extracted exactly 1 PCB zone");
            assert_true(analyzer.get_zones()[0].outline.size() == 5, "Zone outline has 5 vertices");

            // 注册并运行 PCB 分析规则
            PcbRuleRegistry registry;
            registry.register_rule(create_emi_clearance_rule());
            registry.register_rule(create_sensitive_shield_rule());

            Report report = analyzer.analyze(registry);

            std::cout << "[DEBUG] PCB Violations reported:\n";
            for (const auto& v : report.violations) {
                std::cout << "  - [" << v.severity << "] [" << v.rule_id << "] @" << v.location << ": " << v.message << "\n";
            }

            // 验证诊断出的物理 Clearance 隐患
            bool has_emi_clearance_warning = false;
            bool has_fb_crosstalk_warning = false;
            bool has_fb_shield_warning = false;

            for (const auto& v : report.violations) {
                if (v.rule_id == "PCB-01") {
                    has_emi_clearance_warning = true;
                } else if (v.rule_id == "PCB-02") {
                    if (v.message.find("侧向最小边缘间距") != std::string::npos) {
                        has_fb_crosstalk_warning = true;
                    }
                    if (v.message.find("投影区域缺乏完整的 GND 铺铜屏蔽") != std::string::npos) {
                        has_fb_shield_warning = true;
                    }
                }
            }

            assert_true(has_emi_clearance_warning, "Assert PCB-01 high frequency EMI clearance warning is triggered");
            assert_true(has_fb_crosstalk_warning, "Assert PCB-02 FB-to-SW crosstalk coupling warning is triggered (3W violations)");
            assert_true(has_fb_shield_warning, "Assert PCB-02 FB cross-layer shield return-path failure warning is triggered");
        }
    }

    std::cout << "==========================================\n"
              << " S-Expr & Registry & JSON Test Summary: " << passed << " passed, " << failed << " failed.\n"
              << "==========================================\n";
    return failed == 0 ? 0 : 1;
}

} // namespace auditor

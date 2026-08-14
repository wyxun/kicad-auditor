---
name: kicad-auditor
description: KiCad 电路设计协同：用 kicad-auditor 审计引擎读原理图（确定性解析，AI 不直接啃 .kicad_sch 文本）、circuit_calc 精确计算、svg_topology 会话内画拓扑。只读+建议，不修改工程文件。
whenToUse: 需要讨论/校验原理图电路（FB 反馈分压、电源拓扑、运放配置、RC 滤波）、跑全板审计、或用户提到 Buck/Boost/反馈/电源设计时
---

## 设计原则

- **不直接读 .kicad_sch 原始文本**：图形噪音占 90%，连接关系埋在坐标里，AI 直接读既费 token 又易错。一律用工具做语义化提取。
- **只读 + 建议**：默认不修改任何 KiCad 工程文件。给结论、给改值建议，由工程师决定。
- **确定性优先**：数值计算走 `circuit_calc`（精确数学），不用心算估算。

## 三层读取通道

1. **全局摘要**：`audit_sch`（schematic 路径）→ 元件表 + 违规列表（隔离/FB 分压/规格）
2. **局部深入**：`audit_param`（ref + schematic 路径）→ 单元件引脚级网络连接 + 邻近器件 —— 讨论单个电路的标准入口
3. **网表兜底**：KiCad 运行中时 `mcp__kicad__get_netlist` 实时读（需 KiCad 打开工程）

## 计算与绘图

- `circuit_calc`：fb-divider（Vout = VREF×(1+R1/R2)，含 1% 默认公差带、FB 偏置电流、E24 推荐阻值）、buck-ripple、rc-cutoff、sallen-key
- `svg_topology`：buck/boost/fb-divider/rc-filter/sallen-key 拓扑图，labels 传 JSON 值，会话内渲染

## FB 分压校验工作流（S2）

用户说"校验 X 的 FB 分压"时：
1. `audit_sch` 定位 FB 网络相关元件（U1 控制器、分压电阻）
2. `audit_param` 提取上偏/下偏电阻真实值（ref 按需查 R1/R2）
3. `circuit_calc` kind=fb-divider 精确计算 Vout ± 公差带；有目标电压时给 E24 推荐
4. `svg_topology` 画 fb-divider 拓扑标注关键值
5. 结论：偏差百分比 + 改值方案表（E24 替换/串联电阻/换档），并说明 FB 偏置电流假设

## 安全守则

- 只读审计与计算永远安全；`mcp__kicad__` 写操作（place/draw/create/modify 等）系统会弹窗确认，**不自动保存**（saveFrame 由工程师决定）
- 不替工程师修改 .kicad_sch/.kicad_pcb 文件；改板建议以报告/对话形式给出
- 报告按严重级排序（ERROR/WARNING/INFO），先讲会烧板/不能工作的，再讲优化项

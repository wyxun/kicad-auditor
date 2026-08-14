# 华秋 KiCad MCP 连接 SOP

目标：DSH（dsh-mcp-client）以 stdio 方式连接华秋版 KiCad 9.0.7 自带的
kicad-mcp-server，让 AI 实时读取 netlist / 绘制原理图 / ERC / SPICE 仿真。

## 前置条件

- KiCad 华秋版 9.0.7（D:\0_software\KiCad\9.0）**正在运行**
- kicad-mcp-server 依赖就绪（pynng 等；若安装失败见"故障"）
- 目标工程已打开（get_netlist 等工具操作当前帧）

## 步骤

1. **启动 KiCad** 并打开目标工程（原理图/PCB 帧）。

2. **获取 SDK socket URL**：kicad-mcp-server 通过 pynng (nng) IPC 连接 KiCad
   内置 SDK 服务，形如 `ipc:///tmp/kicad_copilot_pair-<uuid>.ipc`。
   来源途径（按序尝试）：
   a. `%APPDATA%\kicad\copilot\site_env.json` / `immutable_settings.json`
      （华秋 copilot 配置）
   b. KiCad 进程命令行 / 日志（copilot 面板）
   c. `netstat -ano | findstr nng` 或监听端口探测
   记录实际 URL 到本文件下方"当前实测"。

3. **配置 dsh-mcp-client**（`C:\Users\Administrator\.dsh\profiles\web\cordis.patch.yml`）：

   ```yaml
   - insert:
       - id: mcp-kicad
         name: '@deepseek-ai/dsh-mcp-client'
         config:
           serverName: kicad
           command: D:/0_software/KiCad/9.0/bin/uv.exe
           args:
             - --directory
             - D:/0_software/KiCad/9.0/bin/kicad-mcp-server
             - run
             - main.py
             - ipc:///tmp/kicad_copilot_pair-<uuid>.ipc
   ```

   键名以 `<DSH>/packages/mcp/mcp-client/README.md` 的配置示例为准。

4. **重启 DSH web**，新会话输入"列出 kicad 相关的 MCP 工具"。
   预期出现 `mcp__kicad__get_netlist`、`mcp__kicad__query_symbol_library`
   （只读）与 `mcp__kicad__place_symbol` 等（写操作每次弹窗确认，由
   kicad-auditor-dsh 插件的 tools/pre-execute 策略保证）。

## 故障

- **pynng 下载 403（清华镜像）**：设 `UV_DEFAULT_INDEX=https://pypi.org/simple`
  或换可用镜像后重跑 `uv sync`/`uv run`。
- **连接失败**：确认 KiCad 运行中且 socket URL 与当前会话一致（KiCad 每次
  启动生成新 IPC 地址，重启 KiCad 后必须更新 patch）。
- **KiCad 未运行时的降级**：域 B 离线通道不受影响——`audit_sch`/`audit_param`
  （kicad-auditor 解析文件）、`circuit_calc`、`svg_topology` 照常可用。

## 当前实测

- [ ] socket URL 已获取：____________（待 KiCad 运行时填写）
- [ ] dsh-mcp-client 连接验证：____________
- [ ] 写工具审批弹窗验证：____________

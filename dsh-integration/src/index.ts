// Aggregated plugin entry for the kicad-auditor-dsh bundle.
// Registers the KiCad circuit-design tools: deterministic audits via
// kicad-auditor.exe, exact circuit math, in-session SVG topology diagrams,
// and the Huaqiu KiCad MCP write-approval policy.
import type { Context } from '@deepseek-ai/cordis'
import z from '@deepseek-ai/schemastery'
import { registerAuditTools } from './kicad/audit.ts'
import { registerCalcTool } from './kicad/calcTool.ts'
import { registerSvgTool } from './kicad/svgTool.ts'
import { isKicadWriteTool } from './kicad/mcpPolicy.ts'
import type { PreToolDecision, ToolExecution } from '@deepseek-ai/dsh-tools'

export const name = 'kicad-auditor-dsh'
export const inject = ['tools', 'subprocess']

/** Plugin configuration; schema in Config validates every field. */
export interface Config {
  /** Working directory for auditor invocations and SVG output. */
  workDir: string
  /** Absolute path to kicad-auditor.exe. */
  auditorPath: string
}

export const Config: z<Config> = z.object({
  workDir: z.string().default('D:/2_xundoc/project/kicad-auditor'),
  auditorPath: z.string().default('D:/2_xundoc/project/kicad-auditor/kicad-auditor.exe'),
})

export function apply(ctx: Context, config: Config): void {
  registerAuditTools(ctx, config)
  registerCalcTool(ctx)
  registerSvgTool(ctx, config)

  // Huaqiu KiCad MCP: write verbs (place/draw/create/modify/...) ask the
  // engineer every call through the tools pipeline's approval seam; read
  // verbs delegate to the chain.
  ctx.on('tools/pre-execute', (exec: ToolExecution, next: () => Promise<PreToolDecision>): Promise<PreToolDecision> => {
    if (isKicadWriteTool(exec.name)) {
      return Promise.resolve({
        kind: 'ask',
        reason: `${exec.name} modifies the open KiCad project; confirm before it runs`,
      })
    }
    return next()
  })
}

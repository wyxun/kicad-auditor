import type { Context } from '@deepseek-ai/cordis'
import { defineTool } from '@deepseek-ai/dsh-tools'
import type { KicadAuditConfig } from './parse.ts'
import { parseAuditJson, buildAuditArgv } from './parse.ts'
import { runTool } from './runner.ts'

const renderText = (_args: never, value: unknown) => [{ type: 'text' as const, text: String(value) }]

/**
 * Register the kicad-auditor tools: deterministic schematic/PCB audit through
 * the local kicad-auditor.exe engine. All commands are read-only; reports and
 * JSON come from the auditor's own -j output.
 */
export function registerAuditTools(ctx: Context, cfg: KicadAuditConfig): void {
  ctx.tools.register(defineTool({
    name: 'audit_sch',
    description:
      'Run kicad-auditor schematic safety audit on a .kicad_sch file (deterministic, read-only). '
      + 'Checks isolation barriers, FB feedback divider ratio/impedance, and component spec limits. '
      + 'Returns structured violations + components for AI interpretation.',
    parameters: {
      schematic: { type: 'string', required: true, description: 'Path to the .kicad_sch file' },
    },
    output: {
      schema: {
        type: 'object',
        properties: {
          violations: { type: 'array', items: { type: 'string' }, required: true },
          components: { type: 'array', items: { type: 'string' }, required: true },
        },
        additionalProperties: false,
      },
      render: (_a, v: unknown) => [{ type: 'text', text: JSON.stringify(v, null, 2) }],
    },
    async execute(args, exec) {
      const r = await runTool(ctx, { exe: cfg.auditorPath, cwd: cfg.workDir, timeoutMs: 60000 }, buildAuditArgv('audit_sch', cfg, args), { signal: exec.signal })
      if (r.exitCode !== 0) throw new Error(`audit_sch failed (exit ${r.exitCode}): ${r.stderr || r.stdout}`)
      const parsed = parseAuditJson(r.stdout)
      if (!parsed) throw new Error(`unrecognized audit output:\n${r.stdout}`)
      return { violations: parsed.violations.map((v) => `[${v.severity}] [${v.rule_id}] @${v.location}: ${v.message}`), components: parsed.components.map((c) => `${c.ref}=${c.value}`) }
    },
  }))

  ctx.tools.register(defineTool({
    name: 'audit_param',
    description:
      'Analyze one component in a schematic: per-pin electrical nets and directly connected components (read-only). '
      + 'The entry point for circuit-design discussion: ask about a specific part (e.g. FB divider) and get its real connections.',
    parameters: {
      ref: { type: 'string', required: true, description: 'Component reference, e.g. "U1" or "R2"' },
      schematic: { type: 'string', required: true, description: 'Path to the .kicad_sch file' },
    },
    output: {
      schema: { type: 'string' },
      render: renderText,
    },
    async execute(args, exec) {
      const r = await runTool(ctx, { exe: cfg.auditorPath, cwd: cfg.workDir, timeoutMs: 60000 }, buildAuditArgv('audit_param', cfg, args), { signal: exec.signal })
      if (r.exitCode !== 0) throw new Error(`audit_param failed (exit ${r.exitCode}): ${r.stderr || r.stdout}`)
      return r.stdout
    },
  }))

  ctx.tools.register(defineTool({
    name: 'audit_pcb',
    description:
      'Run kicad-auditor PCB layout audit on a .kicad_pcb file (read-only): high-frequency switch-node clearance, '
      + 'sensitive-signal (FB) 3W avoidance, and shield reference-plane coverage.',
    parameters: {
      pcb: { type: 'string', required: true, description: 'Path to the .kicad_pcb file' },
    },
    output: {
      schema: { type: 'string' },
      render: renderText,
    },
    async execute(args, exec) {
      const r = await runTool(ctx, { exe: cfg.auditorPath, cwd: cfg.workDir, timeoutMs: 60000 }, buildAuditArgv('audit_pcb', cfg, args), { signal: exec.signal })
      if (r.exitCode !== 0) throw new Error(`audit_pcb failed (exit ${r.exitCode}): ${r.stderr || r.stdout}`)
      return r.stdout
    },
  }))

  ctx.tools.register(defineTool({
    name: 'audit_run',
    description:
      'Run the combined kicad-auditor audit (schematic + PCB + clearance) and produce a Markdown report file. '
      + 'Returns the report path for review.',
    parameters: {
      pcb: { type: 'string', required: true, description: 'Path to the .kicad_pcb file' },
      clearance: { type: 'number', description: 'GND-zone clearance threshold in mm (default 0.2)' },
      report: { type: 'string', description: 'Output report path; defaults next to the PCB file' },
    },
    output: {
      schema: { type: 'object', properties: { reportPath: { type: 'string' } }, additionalProperties: false },
      render: (_a, v: { reportPath: string }) => [{ type: 'text', text: `Report: ${v.reportPath}` }],
    },
    async execute(args, exec) {
      const argv = buildAuditArgv('audit_run', cfg, args)
      const r = await runTool(ctx, { exe: cfg.auditorPath, cwd: cfg.workDir, timeoutMs: 120000 }, argv, { signal: exec.signal })
      if (r.exitCode !== 0) throw new Error(`audit_run failed (exit ${r.exitCode}): ${r.stderr || r.stdout}`)
      const reportPath = args.report as string | undefined
      return { reportPath: reportPath ?? String(args.pcb).replace(/\.kicad_pcb$/i, '.md') }
    },
  }))
}

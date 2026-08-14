export interface AuditViolation {
  rule_id: string
  severity: string
  location: string
  message: string
}

export interface AuditComponent {
  ref: string
  value: string
  pos: { x: number; y: number }
  properties: Record<string, string>
}

export interface AuditResult {
  violations: AuditViolation[]
  components: AuditComponent[]
}

export interface KicadAuditConfig {
  /** Absolute path to kicad-auditor.exe. */
  auditorPath: string
  /** Repository root used as cwd for auditor invocations. */
  workDir: string
}

/**
 * Parse kicad-auditor `-j` stdout into structured results, tolerating
 * [INFO] banner lines that precede the JSON document. Returns empty
 * structures when no JSON object is present, null on malformed JSON.
 */
export function parseAuditJson(text: string): AuditResult | null {
  const start = text.indexOf('{')
  if (start === -1) return { violations: [], components: [] }
  const end = text.lastIndexOf('}')
  if (end <= start) return null
  try {
    const parsed = JSON.parse(text.slice(start, end + 1)) as Partial<AuditResult>
    return {
      violations: Array.isArray(parsed.violations) ? parsed.violations as AuditViolation[] : [],
      components: Array.isArray(parsed.components) ? parsed.components as AuditComponent[] : [],
    }
  } catch {
    return null
  }
}

/**
 * Build argv for one kicad-auditor subcommand from typed tool args.
 * Returns subcommand tokens ONLY: runTool() prepends the executable (same
 * convention as aitrace buildIntrusiveArgv). Including the exe here would
 * spawn `exe exe <cmd> ...` and fail with "Unknown command".
 */
export function buildAuditArgv(toolName: string, _cfg: KicadAuditConfig, args: Record<string, unknown>): string[] {
  switch (toolName) {
    case 'audit_sch': return ['sch', '-i', String(args.schematic), '-j']
    case 'audit_param': return ['param', String(args.ref), String(args.schematic), '-j']
    case 'audit_pcb': return ['pcb', '-i', String(args.pcb), '-j']
    case 'audit_run': {
      const tokens = ['run', '-i', String(args.pcb)]
      if (args.clearance !== undefined) tokens.push('-c', String(args.clearance))
      if (args.report !== undefined) tokens.push('-o', String(args.report))
      return tokens
    }
    default: throw new Error(`unknown audit tool: ${toolName}`)
  }
}

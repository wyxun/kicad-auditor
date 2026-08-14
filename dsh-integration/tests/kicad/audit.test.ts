import { describe, expect, it } from 'vitest'
import { parseAuditJson, buildAuditArgv } from '../../src/kicad/parse.ts'

describe('parseAuditJson', () => {
  it('parses violations and components, tolerating INFO prefix lines', () => {
    const text = [
      '[INFO] Analyzing KiCad Schematic: Buck3.kicad_sch',
      '{',
      '  "violations": [',
      '    { "rule_id": "SCH_ISO_01", "severity": "ERROR", "location": "@U2", "message": "Isolation failure" }',
      '  ],',
      '  "components": [',
      '    { "ref": "U1", "value": "LM2596", "pos": {"x": 1, "y": 2}, "properties": {"Value": "LM2596"} }',
      '  ]',
      '}',
    ].join('\n')
    const r = parseAuditJson(text)
    expect(r).not.toBeNull()
    expect(r!.violations).toHaveLength(1)
    expect(r!.violations[0]).toMatchObject({ rule_id: 'SCH_ISO_01', severity: 'ERROR', location: '@U2' })
    expect(r!.components).toHaveLength(1)
    expect(r!.components[0]).toMatchObject({ ref: 'U1', value: 'LM2596' })
  })

  it('returns null on malformed JSON', () => {
    expect(parseAuditJson('{ not json')).toBeNull()
  })

  it('returns empty structure when no JSON present', () => {
    const r = parseAuditJson('[INFO] nothing here')
    expect(r).not.toBeNull()
    expect(r!.violations).toEqual([])
    expect(r!.components).toEqual([])
  })
})

describe('buildAuditArgv', () => {
  const cfg = { auditorPath: 'D:/auditor/kicad-auditor.exe', workDir: 'D:/proj' }

  // runTool() prepends the exe to the returned tokens; the builder must NOT
  // include it (see aitrace buildIntrusiveArgv for the same convention).
  it('builds sch audit argv with JSON flag, exe provided by runTool', () => {
    expect(buildAuditArgv('audit_sch', cfg, { schematic: 'D:/proj/board.kicad_sch' })).toEqual([
      'sch', '-i', 'D:/proj/board.kicad_sch', '-j',
    ])
  })

  it('builds param argv with ref and schematic, exe provided by runTool', () => {
    expect(buildAuditArgv('audit_param', cfg, { ref: 'U1', schematic: 'D:/proj/board.kicad_sch' })).toEqual([
      'param', 'U1', 'D:/proj/board.kicad_sch', '-j',
    ])
  })

  it('builds pcb audit argv, exe provided by runTool', () => {
    expect(buildAuditArgv('audit_pcb', cfg, { pcb: 'D:/proj/board.kicad_pcb' })).toEqual([
      'pcb', '-i', 'D:/proj/board.kicad_pcb', '-j',
    ])
  })

  it('builds run argv with clearance and report output, exe provided by runTool', () => {
    expect(buildAuditArgv('audit_run', cfg, { pcb: 'D:/proj/board.kicad_pcb', clearance: 0.25, report: 'D:/proj/audit.md' })).toEqual([
      'run', '-i', 'D:/proj/board.kicad_pcb', '-c', '0.25', '-o', 'D:/proj/audit.md',
    ])
  })
})

import { describe, expect, it } from 'vitest'
import { isKicadWriteTool } from '../../src/kicad/mcpPolicy.ts'

describe('isKicadWriteTool', () => {
  it('classifies write tools for approval', () => {
    const writes = [
      'mcp__kicad__place_symbol',
      'mcp__kicad__draw_multi_wires',
      'mcp__kicad__create_local_label',
      'mcp__kicad__modify_symbol_value',
      'mcp__kicad__move_symbol',
      'mcp__kicad__rotate_pcb_footprint',
      'mcp__kicad__set_pcb_footprint_position',
      'mcp__kicad__saveFrame',
    ]
    for (const w of writes) expect(isKicadWriteTool(w), w).toBe(true)
  })

  it('classifies read tools as approval-free', () => {
    const reads = [
      'mcp__kicad__get_netlist',
      'mcp__kicad__get_current_kicad_project',
      'mcp__kicad__query_symbol_library',
      'mcp__kicad__query_pcb_footprint_info',
      'mcp__kicad__runERCCheck',
      'mcp__kicad__showSpiceSimulator',
      'mcp__kicad__exportNetlist',
    ]
    for (const r of reads) expect(isKicadWriteTool(r), r).toBe(false)
  })

  it('leaves non-kicad tools untouched', () => {
    expect(isKicadWriteTool('aitrace_shell')).toBe(false)
  })
})

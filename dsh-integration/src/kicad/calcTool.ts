import type { Context } from '@deepseek-ai/cordis'
import { defineTool } from '@deepseek-ai/dsh-tools'
import { fbDivider, buckInductorRipple, rcCutoff, sallenKeyGain } from './calc.ts'

interface CalcArgs {
  kind: string
  vref?: number
  rTop?: number
  rBottom?: number
  tolerance?: number
  fbCurrent?: number
  targetVout?: number
  vin?: number
  vout?: number
  fsw?: number
  l?: number
  r?: number
  c?: number
  r1?: number
  r2?: number
}

function requireNumber(args: CalcArgs, key: keyof CalcArgs): number {
  const value = args[key]
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new Error(`circuit_calc: parameter "${key}" is required and must be a finite number for kind=${args.kind}`)
  }
  return value
}

/**
 * Register circuit_calc: exact numeric circuit math (not LLM mental math).
 * Computes FB divider (with tolerance band and E24 recommendation), buck
 * inductor ripple, RC cutoff, and Sallen-Key gain.
 */
export function registerCalcTool(ctx: Context): void {
  ctx.tools.register(defineTool({
    name: 'circuit_calc',
    description:
      'Exact circuit math (numeric, not estimation): '
      + 'fb-divider (Vout from VREF + divider with tolerance band and FB bias current; optional E24 recommendation for a target Vout), '
      + 'buck-ripple (inductor ripple current), rc-cutoff (-3dB frequency), sallen-key (non-inverting gain). '
      + 'Pass only the fields relevant to the chosen kind.',
    parameters: {
      kind: { type: 'string', enum: ['fb-divider', 'buck-ripple', 'rc-cutoff', 'sallen-key'], required: true, description: 'Calculation kind' },
      vref: { type: 'number', description: 'fb-divider: FB reference voltage (V)' },
      rTop: { type: 'number', description: 'fb-divider: top resistor R1 (ohm)' },
      rBottom: { type: 'number', description: 'fb-divider: bottom resistor R2 (ohm)' },
      tolerance: { type: 'number', description: 'fb-divider: resistor tolerance as fraction (default 0.01)' },
      fbCurrent: { type: 'number', description: 'fb-divider: FB bias current (A, default 0), positive flows into FB' },
      targetVout: { type: 'number', description: 'fb-divider: optional target Vout to recommend an E24 R2' },
      vin: { type: 'number', description: 'buck-ripple: input voltage (V)' },
      vout: { type: 'number', description: 'buck-ripple: output voltage (V)' },
      fsw: { type: 'number', description: 'buck-ripple: switching frequency (Hz)' },
      l: { type: 'number', description: 'buck-ripple: inductance (H)' },
      r: { type: 'number', description: 'rc-cutoff: resistance (ohm)' },
      c: { type: 'number', description: 'rc-cutoff: capacitance (F)' },
      r1: { type: 'number', description: 'sallen-key: R1 (ohm)' },
      r2: { type: 'number', description: 'sallen-key: R2 (ohm)' },
    },
    output: {
      schema: { type: 'json' },
      render: (_a, v) => [{ type: 'text', text: JSON.stringify(v, null, 2) }],
    },
    async execute(args: CalcArgs) {
      switch (args.kind) {
        case 'fb-divider': {
          return {
            kind: 'fb-divider',
            ...fbDivider({
              vref: requireNumber(args, 'vref'),
              rTop: requireNumber(args, 'rTop'),
              rBottom: requireNumber(args, 'rBottom'),
              tolerance: args.tolerance ?? 0.01,
              fbCurrent: args.fbCurrent ?? 0,
              ...(args.targetVout !== undefined ? { targetVout: args.targetVout } : {}),
            }),
          }
        }
        case 'buck-ripple': {
          return {
            kind: 'buck-ripple',
            ...buckInductorRipple({
              vin: requireNumber(args, 'vin'),
              vout: requireNumber(args, 'vout'),
              fsw: requireNumber(args, 'fsw'),
              l: requireNumber(args, 'l'),
            }),
          }
        }
        case 'rc-cutoff': {
          return {
            kind: 'rc-cutoff',
            ...rcCutoff({ r: requireNumber(args, 'r'), c: requireNumber(args, 'c') }),
          }
        }
        case 'sallen-key': {
          return {
            kind: 'sallen-key',
            ...sallenKeyGain({ r1: requireNumber(args, 'r1'), r2: requireNumber(args, 'r2') }),
          }
        }
        default:
          throw new Error(`circuit_calc: unknown kind "${args.kind}"`)
      }
    },
  }))
}

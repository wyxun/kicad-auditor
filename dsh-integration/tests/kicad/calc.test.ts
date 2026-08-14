import { describe, expect, it } from 'vitest'
import { fbDivider, buckInductorRipple, rcCutoff, sallenKeyGain } from '../../src/kicad/calc.ts'

describe('fbDivider', () => {
  it('computes Vout from VREF and divider with tolerance band', () => {
    const r = fbDivider({ vref: 0.6, rTop: 10000, rBottom: 4700, tolerance: 0.01, fbCurrent: 0 })
    expect(r.voutNominal).toBeCloseTo(1.877, 2)
    expect(r.voutMin).toBeLessThan(r.voutNominal)
    expect(r.voutMax).toBeGreaterThan(r.voutNominal)
  })

  it('applies FB bias current correction (current flowing into FB raises Vout)', () => {
    const r = fbDivider({ vref: 0.6, rTop: 10000, rBottom: 4700, tolerance: 0.01, fbCurrent: 0.5e-6 })
    // Vout = VREF + R1 * (VREF/R2 + Ifb) = 0.6 + 10000 * (0.6/4700 + 0.5e-6)
    expect(r.voutNominal).toBeCloseTo(1.8816, 3)
  })

  it('recommends a standard E24 bottom resistor for a target Vout', () => {
    const r = fbDivider({ vref: 0.6, rTop: 10000, rBottom: 4700, tolerance: 0.01, fbCurrent: 0, targetVout: 1.8 })
    expect(r.recommendedBottom).toBeGreaterThan(0)
    expect(r.voutWithRecommended).toBeGreaterThan(1.7)
    expect(r.voutWithRecommended).toBeLessThan(1.9)
    expect(r.recommendedBottom).toBe(5100) // nearest E24 to 4700*(1.877/1.8-1) path is 5.1k
  })
})

describe('buckInductorRipple', () => {
  it('computes inductor ripple current (V*D/(f*L) form)', () => {
    const r = buckInductorRipple({ vin: 12, vout: 3.3, fsw: 500e3, l: 10e-6 })
    expect(r.dIL).toBeCloseTo((12 - 3.3) * 3.3 / 12 / 500e3 / 10e-6, 6)
    expect(r.dIL).toBeCloseTo(0.4785, 3)
  })
})

describe('rcCutoff', () => {
  it('computes -3dB cutoff frequency', () => {
    const r = rcCutoff({ r: 1000, c: 1e-6 })
    expect(r.fc).toBeCloseTo(1 / (2 * Math.PI * 1000 * 1e-6), 6)
    expect(r.fc).toBeCloseTo(159.15, 1)
  })
})

describe('sallenKeyGain', () => {
  it('computes non-inverting gain 1 + R2/R1', () => {
    const r = sallenKeyGain({ r1: 10000, r2: 10000 })
    expect(r.gain).toBeCloseTo(2, 6)
    expect(r.gainDb).toBeCloseTo(6.02, 1)
  })
})

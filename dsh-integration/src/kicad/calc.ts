/** E24 standard resistor values × decade multipliers. */
const E24 = [1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1]

/** Nearest E24 value (any decade) to a target resistance in ohms. */
export function nearestE24(target: number): number {
  let best = 0
  let bestDiff = Infinity
  for (const base of E24) {
    for (let exp = 0; exp <= 6; exp++) {
      const candidate = base * 10 ** exp
      const diff = Math.abs(candidate - target)
      if (diff < bestDiff) {
        bestDiff = diff
        best = candidate
      }
    }
  }
  return best
}

export interface FbDividerInput {
  /** Reference voltage of the FB pin (V). */
  vref: number
  /** Top divider resistor (R1, Vout side) in ohms. */
  rTop: number
  /** Bottom divider resistor (R2, GND side) in ohms. */
  rBottom: number
  /** Resistor tolerance as a fraction, e.g. 0.01 for 1%. */
  tolerance: number
  /** FB pin bias current in amperes; positive flows INTO the FB pin. */
  fbCurrent: number
  /** Optional target Vout; when present, recommends an E24 bottom resistor. */
  targetVout?: number
}

export interface FbDividerResult {
  voutNominal: number
  voutMin: number
  voutMax: number
  /** Worst-case Vout spread attributable to tolerance, volts. */
  spread: number
  /** Recommended E24 bottom resistor for targetVout (ohms), when requested. */
  recommendedBottom?: number
  /** Vout achieved with the recommended resistor. */
  voutWithRecommended?: number
}

/**
 * Feedback divider analysis: Vout = VREF + R1·(VREF/R2 + Ifb), with worst-case
 * tolerance band (R1 high/R2 low for max; R1 low/R2 high for min).
 */
export function fbDivider(input: FbDividerInput): FbDividerResult {
  const { vref, rTop, rBottom, tolerance, fbCurrent, targetVout } = input
  const nominal = (r1: number, r2: number) => vref + r1 * (vref / r2 + fbCurrent)
  const voutNominal = nominal(rTop, rBottom)
  const voutMax = nominal(rTop * (1 + tolerance), rBottom * (1 - tolerance))
  const voutMin = nominal(rTop * (1 - tolerance), rBottom * (1 + tolerance))
  const result: FbDividerResult = {
    voutNominal,
    voutMin,
    voutMax,
    spread: voutMax - voutMin,
  }
  if (targetVout !== undefined) {
    // R2_target = R1 / (Vout_target / VREF - 1); search nearest E24.
    const r2Target = rTop / (targetVout / vref - 1)
    const recommendedBottom = nearestE24(r2Target)
    result.recommendedBottom = recommendedBottom
    result.voutWithRecommended = nominal(rTop, recommendedBottom)
  }
  return result
}

export interface BuckRippleInput {
  vin: number
  vout: number
  fsw: number
  l: number
}

export interface BuckRippleResult {
  /** Peak-to-peak inductor ripple current in amperes. */
  dIL: number
  /** Duty cycle. */
  duty: number
}

/** Buck inductor ripple: dIL = (Vin−Vout)·D/(f·L), D = Vout/Vin. */
export function buckInductorRipple(input: BuckRippleInput): BuckRippleResult {
  const { vin, vout, fsw, l } = input
  const duty = vout / vin
  const dIL = (vin - vout) * duty / (fsw * l)
  return { dIL, duty }
}

export function rcCutoff(input: { r: number; c: number }): { fc: number } {
  return { fc: 1 / (2 * Math.PI * input.r * input.c) }
}

export function sallenKeyGain(input: { r1: number; r2: number }): { gain: number; gainDb: number } {
  const gain = 1 + input.r2 / input.r1
  return { gain, gainDb: 20 * Math.log10(gain) }
}

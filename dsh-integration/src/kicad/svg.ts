export type TopologyKind = 'fb-divider' | 'buck' | 'boost' | 'rc-filter' | 'sallen-key'

/** Escape a string for safe embedding in SVG/XML text content. */
export function escapeSvgText(text: string): string {
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
}

/** Wrap inner SVG elements into a standalone SVG document. */
export function wrapSvg(width: number, height: number, inner: string): string {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" font-family="sans-serif">${inner}</svg>`
}

// ── primitives ──────────────────────────────────────────────────────────────

const WIRE = '#333'
const NODE = '#1a5fb4'

const label = (x: number, y: number, text: string, anchor: 'start' | 'middle' | 'end' = 'middle', size = 13) =>
  `<text x="${x}" y="${y}" text-anchor="${anchor}" font-size="${size}" fill="#1a1a1a">${escapeSvgText(text)}</text>`

const subLabel = (x: number, y: number, text: string, anchor: 'start' | 'middle' | 'end' = 'middle') =>
  `<text x="${x}" y="${y}" text-anchor="${anchor}" font-size="10.5" fill="#666">${escapeSvgText(text)}</text>`

const wire = (x1: number, y1: number, x2: number, y2: number, color = WIRE) =>
  `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${color}" stroke-width="2"/>`

const junction = (x: number, y: number) =>
  `<circle cx="${x}" cy="${y}" r="3.2" fill="${NODE}"/>`

const nodeDot = (x: number, y: number) =>
  `<circle cx="${x}" cy="${y}" r="2.6" fill="${WIRE}"/>`

/** Horizontal zigzag resistor from (x,y) with total length len, ±6 amplitude. */
function resistorH(x: number, y: number, len: number): string {
  const seg = Math.max(1, Math.floor((len - 8) / 12))
  const pts: number[] = [x, y]
  for (let i = 0; i < seg; i++) {
    pts.push(x + 8 + i * 12, y - 6)
    pts.push(x + 14 + i * 12, y + 6)
  }
  pts.push(x + len, y)
  return `<polyline points="${pts.join(',')}" fill="none" stroke="${WIRE}" stroke-width="2"/>`
}

/** Vertical zigzag resistor from (x,y) with total length len, ±6 amplitude. */
function resistorV(x: number, y: number, len: number): string {
  const seg = Math.max(1, Math.floor((len - 8) / 12))
  const pts: number[] = [x, y]
  for (let i = 0; i < seg; i++) {
    pts.push(x - 6, y + 8 + i * 12)
    pts.push(x + 6, y + 14 + i * 12)
  }
  pts.push(x, y + len)
  return `<polyline points="${pts.join(',')}" fill="none" stroke="${WIRE}" stroke-width="2"/>`
}

/** Vertical capacitor with leads, total length len. */
function capacitorV(x: number, y: number, len: number): string {
  const plate = Math.max(6, len / 3)
  return [
    wire(x, y, x, y + len / 2 - plate / 2),
    `<line x1="${x - 7}" y1="${y + len / 2 - plate / 2}" x2="${x + 7}" y2="${y + len / 2 - plate / 2}" stroke="${WIRE}" stroke-width="2.4"/>`,
    `<line x1="${x - 7}" y1="${y + len / 2 + plate / 2}" x2="${x + 7}" y2="${y + len / 2 + plate / 2}" stroke="${WIRE}" stroke-width="2.4"/>`,
    wire(x, y + len / 2 + plate / 2, x, y + len),
  ].join('')
}

/** Horizontal inductor: three half-arcs, total length len. */
function inductorH(x: number, y: number, len: number): string {
  const seg = len / 3
  const r = seg / 2
  let d = `M ${x} ${y}`
  for (let i = 0; i < 3; i++) {
    d += ` a ${r} ${r} 0 0 1 ${seg} 0`
  }
  return `<path d="${d}" fill="none" stroke="${WIRE}" stroke-width="2" stroke-linecap="round"/>`
}

/** Vertical diode pointing down (anode top, cathode bottom). */
function diodeV(x: number, y: number, len: number): string {
  const h = len - 12
  const top = y + 6
  return [
    wire(x, y, x, top),
    `<polygon points="${x},${top} ${x + 8},${top + h / 2} ${x},${top + h}" fill="none" stroke="${WIRE}" stroke-width="2"/>`,
    `<line x1="${x}" y1="${top + h - 4}" x2="${x}" y2="${top + h + 4}" stroke="${WIRE}" stroke-width="2.4"/>`,
    wire(x, top + h + 4, x, y + len),
  ].join('')
}

/** Ground symbol: three descending bars. */
const gnd = (x: number, y: number) =>
  `<line x1="${x - 12}" y1="${y}" x2="${x + 12}" y2="${y}" stroke="${WIRE}" stroke-width="2.4"/>`
  + `<line x1="${x - 8}" y1="${y + 5}" x2="${x + 8}" y2="${y + 5}" stroke="${WIRE}" stroke-width="2"/>`
  + `<line x1="${x - 4}" y1="${y + 10}" x2="${x + 4}" y2="${y + 10}" stroke="${WIRE}" stroke-width="1.6"/>`

/** Op-amp triangle with inverting (−) and non-inverting (+) inputs. */
function opamp(x: number, y: number, w: number, h: number): string {
  const half = h / 2
  return [
    `<polygon points="${x},${y} ${x + w},${y + half} ${x},${y + h}" fill="#f5f5f5" stroke="${WIRE}" stroke-width="2"/>`,
    wire(x - 18, y + 8, x, y + 8),
    label(x - 24, y + 12, '−', 'end'),
    wire(x - 18, y + h - 8, x, y + h - 8),
    label(x - 24, y + h - 4, '+', 'end'),
    wire(x + w, y + half, x + w + 18, y + half),
  ].join('')
}

/** Switch: open contacts — fixed end and movable end both on the y line,
 * lever drawn as a raised diagonal between them (IEEE-style open contact). */
function switchH(x: number, y: number, len: number): string {
  return [
    wire(x, y, x + 8, y),
    `<line x1="${x + 8}" y1="${y}" x2="${x + len - 8}" y2="${y - 16}" stroke="${WIRE}" stroke-width="2" stroke-linecap="round"/>`,
    nodeDot(x + len - 8, y),
    wire(x + len - 8, y, x + len, y),
  ].join('')
}

const vLabel = (x: number, y: number, text: string, anchor: 'start' | 'middle' | 'end' = 'middle') =>
  `<text x="${x}" y="${y}" text-anchor="${anchor}" font-size="13" font-weight="bold" fill="#0b3d91">${escapeSvgText(text)}</text>`

// ── topologies ──────────────────────────────────────────────────────────────

/**
 * Render a parameterized circuit topology as SVG. Labels carry the concrete
 * values the AI wants to show; geometry is a textbook-style schematic sketch.
 */
export function renderSvgTopology(kind: TopologyKind, labels: Record<string, string>): string {
  const l = (key: string) => labels[key] ?? key
  switch (kind) {
    case 'fb-divider': {
      // VIN ── R1 ──┬── FB/VOUT ; R2 ── GND
      const x = 90
      const yTop = 46
      const yFb = 150
      const yR2Top = 200
      const yBot = 296
      const xOut = 210
      const inner = [
        vLabel(x, 26, l('vin')),
        junction(x, yTop),
        wire(x, yTop + 3, x, yTop + 10),
        resistorV(x, yTop + 10, 70),
        wire(x, yTop + 80, x, yFb),
        junction(x, yFb),
        wire(x, yFb, xOut, yFb),
        nodeDot(xOut, yFb),
        vLabel(xOut + 6, yFb - 10, l('vout'), 'start'),
        label(xOut + 6, yFb + 14, l('fb'), 'start'),
        subLabel(xOut + 6, yFb + 28, labels['vref'] ? `VREF ${labels['vref']}` : '', 'start'),
        wire(x, yFb, x, yR2Top),
        resistorV(x, yR2Top, 70),
        wire(x, yR2Top + 70, x, yBot),
        gnd(x, yBot),
        label(x + 26, 92, l('r1'), 'start'),
        label(x + 26, 240, l('r2'), 'start'),
      ].join('')
      return wrapSvg(320, 330, inner)
    }
    case 'buck': {
      // VIN ── SW ──┬── L ── VOUT ; D ── GND ; C ── GND ; FB divider
      const yMain = 90
      const xVin = 60
      const xSw = 150
      const xL = 250
      const xOut = 330
      const inner = [
        vLabel(xVin, yMain - 22, l('vin')),
        junction(xVin, yMain),
        wire(xVin + 3, yMain, xSw - 30, yMain),
        switchH(xSw - 30, yMain, 60),
        wire(xSw + 30, yMain, xL - 22, yMain),
        inductorH(xL - 22, yMain, 44),
        wire(xL + 22, yMain, xOut, yMain),
        junction(xOut, yMain),
        vLabel(xOut + 4, yMain - 22, l('vout'), 'start'),
        // freewheeling diode: SW node down to GND
        wire(xSw, yMain, xSw, yMain + 20),
        diodeV(xSw, yMain + 20, 46),
        wire(xSw, yMain + 66, xSw, yMain + 80),
        gnd(xSw, yMain + 80),
        label(xSw + 16, yMain + 46, l('d'), 'start'),
        // output cap: VOUT node down to GND
        wire(xOut, yMain, xOut, yMain + 20),
        capacitorV(xOut, yMain + 20, 46),
        wire(xOut, yMain + 66, xOut, yMain + 80),
        gnd(xOut, yMain + 80),
        label(xOut + 16, yMain + 46, l('c'), 'start'),
        // FB divider branch from VOUT
        wire(xOut, yMain, xOut + 40, yMain),
        junction(xOut + 40, yMain),
        wire(xOut + 40, yMain, xOut + 40, yMain + 26),
        resistorV(xOut + 40, yMain + 26, 40),
        wire(xOut + 40, yMain + 66, xOut + 40, yMain + 80),
        gnd(xOut + 40, yMain + 80),
        label(xOut + 52, yMain + 48, l('fb'), 'start'),
        // inductor label above
        label(xL, yMain - 20, l('l')),
      ].join('')
      return wrapSvg(430, 220, inner)
    }
    case 'boost': {
      // VIN ── L ──┬── D ── VOUT ; SW ── GND ; C ── GND
      const yMain = 80
      const xVin = 50
      const xL = 130
      const xSw = 210
      const xOut = 300
      const inner = [
        vLabel(xVin, yMain - 20, l('vin')),
        junction(xVin, yMain),
        wire(xVin + 3, yMain, xL - 22, yMain),
        inductorH(xL - 22, yMain, 44),
        wire(xL + 22, yMain, xSw, yMain),
        junction(xSw, yMain),
        // switch down to GND
        wire(xSw, yMain, xSw, yMain + 12),
        switchH(xSw - 20, yMain + 12, 40),
        wire(xSw + 20, yMain + 12, xSw + 20, yMain + 40),
        gnd(xSw + 20, yMain + 40),
        label(xSw + 34, yMain + 24, l('sw'), 'start'),
        // diode to output
        wire(xSw, yMain, xSw + 14, yMain),
        diodeV(xSw + 14, yMain, 40),
        wire(xSw + 14, yMain + 40, xSw + 14, yMain + 54),
        wire(xSw + 14, yMain + 54, xOut, yMain + 54),
        junction(xOut, yMain + 54),
        vLabel(xOut + 4, yMain + 44, l('vout'), 'start'),
        // output cap
        wire(xOut, yMain + 54, xOut, yMain + 70),
        capacitorV(xOut, yMain + 70, 40),
        wire(xOut, yMain + 110, xOut, yMain + 124),
        gnd(xOut, yMain + 124),
        label(xOut + 16, yMain + 96, l('c'), 'start'),
        label(xL, yMain - 20, l('l')),
      ].join('')
      return wrapSvg(380, 240, inner)
    }
    case 'rc-filter': {
      // VIN ── R ──┬── VOUT ; C ── GND
      const yMain = 80
      const xVin = 50
      const xR = 140
      const xOut = 240
      const inner = [
        vLabel(xVin, yMain - 20, l('vin')),
        junction(xVin, yMain),
        wire(xVin + 3, yMain, xR - 20, yMain),
        resistorH(xR - 20, yMain, 40),
        wire(xR + 20, yMain, xOut, yMain),
        junction(xOut, yMain),
        vLabel(xOut + 4, yMain - 20, l('vout'), 'start'),
        wire(xOut, yMain, xOut, yMain + 16),
        capacitorV(xOut, yMain + 16, 42),
        wire(xOut, yMain + 58, xOut, yMain + 74),
        gnd(xOut, yMain + 74),
        label(xOut + 16, yMain + 40, l('c'), 'start'),
        label(xR, yMain - 20, l('r')),
      ].join('')
      return wrapSvg(300, 200, inner)
    }
    case 'sallen-key': {
      // Non-inverting amp: VIN ── (+); R2: OUT ── (−); R1: (−) ── GND.
      // Gain = 1 + R2/R1.
      const xIn = 60
      const yIn = 60
      const xOp = 210
      const yOp = 60
      const xOut = 340
      const yMinus = 38
      const yPlus = 82
      const inner = [
        vLabel(xIn, yIn - 24, l('vin')),
        junction(xIn, yIn),
        wire(xIn + 3, yIn, 150, yIn),
        wire(150, yIn, 150, yPlus),
        wire(150, yPlus, xOp - 18, yPlus),
        // op-amp (+ at yPlus, − at yMinus)
        opamp(xOp, yOp - 30, 70, 60),
        label(xOp - 6, yOp - 18, l('opamp'), 'end'),
        // output
        wire(xOp + 70, yOp, xOut, yOp),
        vLabel(xOut + 4, yOp - 24, l('vout'), 'start'),
        junction(xOut, yOp),
        // feedback R2: (−) up to y=20, across to output rail
        wire(xOp - 18, yMinus, xOp - 18, 20),
        resistorH(xOp - 18, 20, 68),
        wire(xOp + 50, 20, xOp + 50, yOp),
        wire(xOp + 50, yOp, xOp + 70, yOp),
        label(xOp + 16, 10, l('r2')),
        // R1: (−) down to GND
        wire(xOp - 18, yMinus, xOp - 18, 116),
        resistorV(xOp - 18, 116, 50),
        wire(xOp - 18, 166, xOp - 18, 192),
        gnd(xOp - 18, 192),
        label(xOp - 2, 146, l('r1'), 'start'),
      ].join('')
      return wrapSvg(390, 230, inner)
    }
    default: {
      const exhaustive: never = kind
      throw new Error(`svg_topology: unknown kind ${String(exhaustive)}`)
    }
  }
}

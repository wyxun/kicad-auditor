import { describe, expect, it } from 'vitest'
import { escapeSvgText, wrapSvg, renderSvgTopology } from '../../src/kicad/svg.ts'

describe('escapeSvgText', () => {
  it('escapes XML special characters', () => {
    expect(escapeSvgText('R1 <10k & "A" > \'B\'')).toBe('R1 &lt;10k &amp; &quot;A&quot; &gt; &#39;B&#39;')
  })
})

describe('wrapSvg', () => {
  it('produces a standalone SVG document with viewBox', () => {
    const svg = wrapSvg(400, 200, '<rect x="0" y="0" width="10" height="10"/>')
    expect(svg).toContain('<svg')
    expect(svg).toContain('viewBox="0 0 400 200"')
    expect(svg).toContain('width="400"')
    expect(svg).toContain('</svg>')
  })
})

describe('renderSvgTopology', () => {
  it('renders a fb-divider topology with labeled values', () => {
    const svg = renderSvgTopology('fb-divider', { vin: '5V', r1: '10k', fb: 'VREF 0.6V', r2: '4.7k' })
    expect(svg).toContain('5V')
    expect(svg).toContain('10k')
    expect(svg).toContain('VREF 0.6V')
    expect(svg).toContain('4.7k')
  })

  it('renders a buck topology with labels', () => {
    const svg = renderSvgTopology('buck', { vin: '12V', vout: '3.3V', l: '10µH', fb: 'FB' })
    expect(svg).toContain('12V')
    expect(svg).toContain('3.3V')
    expect(svg).toContain('10µH')
    expect(svg).toContain('FB')
  })

  it('throws on unknown kind', () => {
    expect(() => renderSvgTopology('nope' as never, {})).toThrow()
  })
})

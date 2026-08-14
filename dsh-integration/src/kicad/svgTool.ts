import type { Context } from '@deepseek-ai/cordis'
import { defineTool } from '@deepseek-ai/dsh-tools'
import { mkdir, writeFile } from 'node:fs/promises'
import { join } from 'node:path'
import { renderSvgTopology, type TopologyKind } from './svg.ts'

/**
 * Register svg_topology: renders a parameterized circuit topology as an SVG
 * file inside the session workspace, returning its path for in-session
 * rendering (zero risk; never touches the KiCad project files).
 */
export function registerSvgTool(ctx: Context, cfg: { workDir: string }): void {
  ctx.tools.register(defineTool({
    name: 'svg_topology',
    description:
      'Render a circuit topology (buck/boost/fb-divider/rc-filter/sallen-key) as an SVG diagram file. '
      + 'Pass `labels` as a JSON object mapping label keys (vin, vout, r1, r2, fb, l, c, d, sw, opamp, in+, r1, r2) to the concrete values to show. '
      + 'Returns the SVG path; the diagram renders in-session. Use for design discussions before touching the KiCad project.',
    parameters: {
      kind: {
        type: 'string',
        enum: ['fb-divider', 'buck', 'boost', 'rc-filter', 'sallen-key'],
        required: true,
        description: 'Topology kind',
      },
      labels: {
        type: 'string',
        required: true,
        description: 'JSON object of label values, e.g. {"vin":"12V","vout":"3.3V","r1":"10k","r2":"4.7k"}',
      },
    },
    output: {
      schema: { type: 'object', properties: { svgPath: { type: 'string' } }, additionalProperties: false },
      render: (_a, v: { svgPath: string }) => [{
        type: 'text',
        text: `![](file:///${v.svgPath.replace(/\\/g, '/')})\nDiagram saved to ${v.svgPath}`,
      }],
    },
    async execute(args) {
      let labels: Record<string, string>
      try {
        const parsed = JSON.parse(args.labels) as unknown
        if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
          throw new Error('labels must be a JSON object')
        }
        labels = parsed as Record<string, string>
      } catch (e) {
        throw new Error(`svg_topology: invalid labels JSON: ${(e as Error).message}`)
      }
      const svg = renderSvgTopology(args.kind as TopologyKind, labels)
      const dir = join(cfg.workDir, 'diagrams')
      await mkdir(dir, { recursive: true })
      const name = `topo_${args.kind}_${new Date().toISOString().replace(/[:.]/g, '-')}.svg`
      const svgPath = join(dir, name)
      await writeFile(svgPath, svg, 'utf8')
      return { svgPath }
    },
  }))
}

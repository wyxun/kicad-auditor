/**
 * Write-verb detection for Huaqiu KiCad MCP tools. Write operations modify the
 * open KiCad project (place/draw/create/modify/move/rotate/set/delete/save);
 * everything else (get/query/export/run/show/open/close) is read-only.
 * Unknown mcp__kicad__ tools default to write (fail closed for safety).
 */
export function isKicadWriteTool(toolName: string): boolean {
  if (!toolName.startsWith('mcp__kicad__')) return false
  const verb = toolName.slice('mcp__kicad__'.length)
  if (/^(get|query|export|run|show|open|close|toggle|zoom|select|unSelect)/.test(verb)) return false
  return true
}

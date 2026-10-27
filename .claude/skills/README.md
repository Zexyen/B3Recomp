# Third-party skills and plugins

| What | Source | Pinned to | License | Installed via |
|---|---|---|---|---|
| `better-design` skill | https://github.com/marvkr/better-design (`skills/better-design`) | `175e7ba` | MIT, see `better-design/LICENSE` | Vendored here |
| Better Design MCP server | `https://better-design.com/api/mcp` | hosted | n/a | `/.mcp.json` |
| `ponytail` plugin (6 skills, lifecycle hooks, status band) | https://github.com/DietrichGebert/ponytail | `09c434a` | MIT | `/.claude/settings.json` (marketplace + `enabledPlugins`) |

## Setup

- **Better Design** needs an API key in the `BETTER_DESIGN_API_KEY` environment variable (cloud: add it as an environment secret; local: export it). Get one by running `npx better-design` once. Without it the MCP server fails to connect and the skill says so.
- **ponytail** installs when a session trusts this repo's settings. Its hooks need `node` on PATH. It turns on at the `full` level every session (`PONYTAIL_DEFAULT_MODE=off` disables that) and writes a per-task usage row to `~/.config/ponytail/band/`. Its optional webhook is off unless configured.

## Updating

- better-design skill: re-copy `skills/better-design/SKILL.md` from upstream and bump the commit above.
- ponytail: change `ref` in `.claude/settings.json`.

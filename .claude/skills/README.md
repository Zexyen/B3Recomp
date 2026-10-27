# Vendored skills

Third-party Claude Code skills, copied as plain files so every session in this repo picks them up.

| Skill(s) | Source | Commit | License |
|---|---|---|---|
| `better-design` | https://github.com/marvkr/better-design (`skills/better-design`) | `175e7ba` | MIT, see `better-design/LICENSE` |
| `ponytail`, `ponytail-review`, `ponytail-audit`, `ponytail-debt`, `ponytail-gain`, `ponytail-help` | https://github.com/DietrichGebert/ponytail (`skills/`) | `09c434a` | MIT, see `ponytail/LICENSE` |

Notes:

- `better-design` is a workflow layer over the Better Design MCP server (`https://better-design.com/api/mcp`). Without that MCP connected, the skill only tells you it isn't connected.
- Only ponytail's skills are vendored, not its Claude Code plugin (Node lifecycle hooks, status band, usage ledger). Without the hooks, ponytail applies when a skill is invoked rather than being auto-on every session, and level switching doesn't persist.
- To update, re-copy the `SKILL.md` files from the upstream repos and bump the commits above.

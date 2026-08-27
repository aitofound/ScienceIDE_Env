# Repository root layout

The repository root is an interface. Keep stable authoring, automation, and data
entry points visible; place historical or dated records under named namespaces.

| Path | Owner and purpose |
| --- | --- |
| `tasks/` | Canonical task packages. A new Harbor leaf may be direct or one logistics layer deep. |
| `skills/` | Canonical agent-facing authoring procedures. |
| `scripts/` | Repository validation, projection, grading, and image tooling. |
| `registry/` | Registry source data and generated YAML projections. |
| `registry.json` | Generated root compatibility projection. It stays at the root until every external reader is audited. |
| `runs/` | Dated execution products, skills, verdicts, and run receipts. These are records, not task definitions. |
| `archive/` | Retired task packages that retain stable slug paths. `archive/templates/` holds historical templates. |
| `docs/history/` | Human-readable historical campaign records and handoffs. |
| `.github/` | GitHub issue, label, and workflow integration. |
| `.claude/` | Thin Claude skill-discovery redirects. |
| Root Markdown files | Human and agent entry points: `README.md`, `AGENTS.md`, `CONTRIBUTING.md`, and `LICENSE`. |
| `package.json`, `package-lock.json` | Reproducible local and CI commands/dependencies. |

## Invariants

- Moving a file must not change task science, scoring, package content, or run
  evidence.
- `tasks/`, `skills/`, `scripts/`, and `registry/` are stable public interfaces;
  do not move them merely to reduce the number of root entries.
- Retired task directories remain directly under `archive/` so their slug paths
  stay stable. Historical templates live under `archive/templates/` and are not
  active submission entry points.
- `runs/` may be excluded from solver-facing checkouts, but tracked run records
  remain immutable evidence.
- No compatibility projection moves until its external consumers are identified
  and updated in the same change.
- Prefer `git mv` plus reference updates. Never delete historical material as a
  form of tidying.

## Current organization change

The first root-layout cleanup makes only structural moves:

- `TEMPLATE/` → `archive/templates/grid-v1/`
- `submissions/` → `runs/`
- GPU campaign handoff/results → `docs/history/gpu/`

All content is preserved. The active authoring entrance is
`skills/package-sciaccel-task/SKILL.md`.

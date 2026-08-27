# ScienceAccelBench task finalization checklist

Use this at the finalization boundary, after the Harbor leaf and its checks are
implemented and before requesting review. Copy it into working notes, replace
the placeholders, and mark every non-applicable item with a reason.

`BLOCKER` means stop before review or the named side effect. `RECORD` means
capture the evidence and its source. `IF APPLICABLE` and `OPTIONAL` are
context-dependent; do not silently skip them.

## Fill-in header

| Field | Value |
| --- | --- |
| Task / slug | `<tasks/<group>/<module-slug>>` |
| Science owner | `<name / contact>` |
| Reviewer | `<name / contact>` |
| Base commit | `<full SHA>` |
| Source pin | `<repo_commit or archive digest>` |
| Target(s) | `<active target descriptor(s)>` |
| Validation environment | `<host/container, CPU/GPU/device, tool versions>` |
| Decision / authorization links | `<owner decision, review, and side-effect approvals>` |

## 1. Scope, identity, and provenance

- [ ] BLOCKER — Confirm this is one independent scientific or numerical module;
  split unrelated modules into separate Harbor leaves.
- [ ] BLOCKER — Confirm the slug is globally unique and the header identifies
  the owner, reviewer, base commit, source pin, target(s), and validation
  environment.
- [ ] BLOCKER — Prove the exact source bytes: record the full commit or archive
  digest, the relevant tree/blob evidence, and the source/build configuration
  used for validation.
- [ ] BLOCKER — Audit the closed leaf root: only `task.toml`, `instruction.md`,
  `code/`, `environment/`, `tests/`, `solution/`, `target/`, and optional
  `comment/` are present.
- [ ] BLOCKER — Confirm `code/` has exactly one direct real
  `code/<codebasename>/` child containing the vendored codebase; use no
  symlink, external checkout, parent/sibling file, or out-of-task dependency.
- [ ] BLOCKER — Confirm no tracked build output, cache, log, oracle result,
  temporary file, or other packaging garbage is present.
- [ ] RECORD — Note source provenance, scope decisions, hazards, exclusions,
  and any unresolved ownership or authorization question.

## 2. Contract ownership and manifest minimality

| Canonical owner | Final audit |
| --- | --- |
| `task.toml` | Concise allowed registry/discovery metadata only; no copied operational, numerical, or history evidence. |
| `instruction.md` | The normative solver-facing module, interface, input, output, target, and invocation contract. |
| `tests/checks/<check>/` | The exact check-owned deck/fixtures, rubric, validator, artifact requirements, and pass policy. |
| `solution/` | The trusted original CPU/oracle preparation path. |
| `comment/` | Optional, repository-visible, runtime-hidden, non-normative evidence and preparation narrative. |
| `target/*.json` | Flat strict device/module/code/environment facts; every active descriptor is supported by the instruction. |

- [ ] BLOCKER — Assign each contract fact one canonical owner in the map;
  remove or reconcile duplicated policy rather than leaving two authorities.
- [ ] BLOCKER — Keep `task.toml` to registry/discovery facts and the repository's
  allowed vocabulary; allowed metadata is not automatically required metadata.
- [ ] BLOCKER — Route detailed operational, numerical, calibration, and history
  evidence to the check rubric/validator or `comment/` owner instead of
  duplicating it in the manifest or public instruction.
- [ ] RECORD — List any intentionally repeated summary and link it to its
  canonical owner.
- [ ] OPTIONAL — Add a short reviewer summary or diagram only when it improves
  navigation; keep it non-normative and link it to the canonical facts.

## 3. Check portfolio and reward audit

- [ ] BLOCKER — Use ordinary stable direct names under `tests/checks/`; ensure
  direct `check.json` metadata, when present, has only a unique nonempty
  lower-kebab-case `labels` array.
- [ ] BLOCKER — Ensure at least one direct check carries the exact
  `acceleration` label and has a materially worthwhile workload.
- [ ] BLOCKER — Show that the portfolio covers the module's meaningful breadth,
  not only its easiest or most convenient kernel.
- [ ] BLOCKER — Confirm `tests/test.sh` is the only verifier entrance and emits
  Harbor's non-binary reward after the CPU-equivalence policy passes.
- [ ] RECORD — Write the reward aggregation equation, every check weight, the
  possible reward values/ranges, costly-check weighting rationale,
  binary-versus-continuous behavior, failure semantics, active target count,
  and resulting check-by-target cell count.
- [ ] RECORD — Explain what each check catches, what a cheap non-port shortcut
  would be, and which check, fixture, or validator rejects it.

## 4. Deck ↔ rubric ↔ validator ↔ instruction matrix

Fill every cell or mark it `N/A — <reason>`. Values must be identical in all
applicable owners, with the source of the value recorded in the last column.

| Contract field | Deck / inputs | Rubric | Validator | `instruction.md` / target | Evidence / status |
| --- | --- | --- | --- | --- | --- |
| `tmax` / `dtout` | `<...>` | `<...>` | `<...>` | `<...>` | `<source / verified>` |
| Grids / resolution | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Dimensions | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Variables / fields | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Dtype / order / layout | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Required artifacts | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Eligible frames / frame count | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Tolerances / comparison rule | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Timestep policy | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Targets / active descriptors | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |
| Filenames / paths | `<...>` | `<...>` | `<...>` | `<...>` | `<...>` |

- [ ] BLOCKER — Resolve every mismatch before review; do not make a deck,
  rubric, validator, and instruction agree only by prose.

## 5. Evidence provenance and claim strength

- [ ] BLOCKER — For every measured claim, record the exact configuration,
  source pin, window, build/compiler pair, hardware, and output set that
  produced it.
- [ ] BLOCKER — If a deck or graded window changes, rerun correctness
  calibration for that configuration or explicitly downgrade the statement to
  bounded extrapolation; never silently call it the “exact deck.”
- [ ] BLOCKER — Separate evidence for viability, the CPU/oracle self-test,
  correctness calibration, and performance; do not use one as proof of another.
- [ ] RECORD — Capture commands, configuration files, build identifiers,
  measured values, exit codes, warnings, owner decisions, and blind spots.
- [ ] IF APPLICABLE — When incumbent runs differ or a numerical floor is being
  measured, classify and document the warranted claim using
  [`references/determinism-triage.md`](../references/determinism-triage.md);
  do not invent a registry-wide tolerance or determinism rule.

## 6. Numerical and validator policy

- [ ] BLOCKER — Keep criteria check-owned and calibrate claim strength to
  measurement or the science owner's decision; do not guess tolerances.
- [ ] BLOCKER — Make the validator read authoritative rubric values rather than
  maintaining a second threshold or timestep policy in validator code.
- [ ] BLOCKER — Check format, non-finite values, shape/layout, filenames, and
  every required artifact before applying scientific comparisons.
- [ ] BLOCKER — Run discrimination fixtures with both required accepts and
  rejects; include a correct-but-different accept case when the policy permits
  it, or record why it is not applicable.
- [ ] BLOCKER — Remove stale or contradictory hidden/public policy prose;
  leave each rule at its canonical owner.
- [ ] RECORD — Record fixture names, expected verdicts, observed verdicts, and
  any known blind spot or untested exploit.

## 7. Oracle self-test and resource evidence

- [ ] BLOCKER — From the leaf, run the trusted CPU path and the same verifier:

  ```bash
  ./solution/solve.sh
  ./tests/test.sh
  ```

- [ ] BLOCKER — Record the exact commands and exit codes; claim no oracle,
  GPU, validator, or verifier result as passing unless it actually ran.
- [ ] IF APPLICABLE — Validate the mainline/resource workload at its declared
  scale and record actual process/device use, output count, wall time, CPU
  evidence, and relevant resource limits.
- [ ] IF APPLICABLE — Label each failure as infrastructure/environment or
  solver/science, with the evidence and rerun decision; do not conflate them.

## 8. Repository gates and final tree audit

- [ ] BLOCKER — Confirm generated registry projections are fresh; do not
  hand-edit generated files.
- [ ] BLOCKER — Confirm `task.toml`, target descriptors, and direct check
  metadata parse, and that active targets are flat strict JSON. The commands
  below are the repository/Harbor gates for these contracts.
- [ ] BLOCKER — Run and record each applicable command and exit code:

  ```bash
  node scripts/gen-index.mjs --check
  python3 skills/package-sciaccel-task/scripts/validate-harbor-task.py \
    tasks/<group>/<module-slug>
  python3 skills/package-sciaccel-task/scripts/validate-harbor-task.py --all tasks
  BASE_REF=origin/main CHECK_LINKS=1 npm run check
  npm run check:validators
  git diff --check
  ```

- [ ] BLOCKER — Scan the final diff for merge-conflict markers and unintended
  control characters; record the exact scan and result.
- [ ] BLOCKER — Audit `git status --short --untracked-files=all`,
  `git diff --name-status`, and the tracked tree for unexpected files,
  generated drift, secrets, caches, logs, or outputs.
- [ ] RECORD — List the final Harbor leaves/targets/cells validated and any
  command that was skipped, blocked, or only exercised in a reduced environment.

## 9. Git, GitHub, and side-effect preflight

- [ ] BLOCKER — Before commit, push, or PR creation, verify the intended Git
  author identity, GitHub identity/permissions, remote, branch, base, and
  changed paths using read-only checks.
- [ ] BLOCKER — Confirm explicit authorization for each external side effect;
  do not infer permission from the task request or from repository access.
- [ ] BLOCKER — Make the PR body state scope, tests/commands and exit codes,
  evidence-backed known gaps, and any skipped validation.
- [ ] BLOCKER — Do not merge without separate review and merge authority.

## Jason's closeout rule — required

> After a real task-packaging cycle, if the agent learned a reusable lesson
> that would help future task authors, improve `skills/package-sciaccel-task/`
> and open a separate evidence-backed PR for that skill improvement. Keep
> task-specific facts and evidence out of the generic asset; do not silently
> bundle the skill improvement into the task PR. Validate the skill and
> repository, and do not merge the skill PR without review/authorization.

- [ ] IF APPLICABLE — Record the reusable lesson, its evidence, the separate
  skill-PR link, and the skill/repository validation results.
- [ ] RECORD — If no reusable lesson was learned, record that decision and why;
  otherwise complete the separate skill-PR path above.

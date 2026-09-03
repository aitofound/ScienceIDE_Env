# Benchmark Follow-Up Polish Design

## Context

Release `v0.1.1` publishes the bounded H2O/cc-pVDZ all-electron benchmark at
the CI-verified commit `025a6dd`. Three follow-up polish items remain:

1. make all five independent benchmark runs machine-readable;
2. describe the memory option as a conservative preflight budget rather than
   an operating-system hard limit;
3. surface the reviewer follow-up benchmark in Quantum Harness PR #217.

The published `v0.1.1` tag is immutable and must continue to point to
`025a6dd`. These follow-up changes land on `main`.

## Considered Approaches

### 1. Versioned summary artifact plus backward-compatible CLI alias

Commit a summary JSON containing every raw timing/RSS observation and
recomputed aggregates. Rename the canonical option to
`--memory-budget-gib`, retain `--max-memory-gib` as a visible alias, and append
one benchmark section to the current PR body.

This is the selected design. It is reviewable, preserves old reproduction
commands, and does not reinterpret the measurements.

### 2. Repeat the benchmark inside one Rust process

This could generate aggregates automatically, but it would no longer match the
documented five-fresh-process measurement policy. Allocator reuse and a single
process-level peak RSS would make the result materially different.

### 3. Add a cross-platform benchmark orchestration framework

A runner could launch child processes and normalize macOS/Linux RSS tools.
That is useful future work, but it is larger than the approved three-item
polish and would introduce additional platform-specific behavior.

## Summary JSON Contract

Create `fixtures/h2o-ccpvdz-ae/benchmark-m4-summary.json` with:

- schema and artifact-kind identifiers;
- measured source commit and release tag;
- hardware, operating-system, Rust, and build metadata;
- exact command and independent-process policy;
- all five raw stage timings, wall times, peak RSS values, contribution
  throughputs, and deterministic checksums;
- median stage timings, median wall time, median and maximum RSS, and median
  contribution throughput.

Tests must load the artifact, require exactly five runs, recompute every median
and maximum from raw values, and verify invariant checksums and determinant
metadata against `benchmark-m4.json`.

## Memory Option Contract

The canonical CLI is:

```text
--memory-budget-gib 2
```

The help text must say that the option rejects a run when the conservative
preflight estimate exceeds the budget and is not an operating-system hard
memory limit.

The previous option remains valid:

```text
--max-memory-gib 2
```

It is a visible compatibility alias. Both spellings must produce the same
preflight rejection before integral or link-table construction.

## PR Body Update

Preserve the complete existing PR body. Change its Release row from `v0.1.0`
to `v0.1.1`, append a `Reviewer follow-up benchmark` section with measured
results and immutable links, and update the checklist to reference the public
`v0.1.1` source.

No existing poem, translation, image, team name, acceptance result, or scope
statement is removed.

## Verification

- JSON parsing and recomputed aggregate tests pass.
- CLI help exposes the canonical option, alias, and precise limitation.
- Both option spellings reject a 0.5 GiB budget before large allocations.
- README, report, and reproduction commands use the canonical option.
- Full formatting, Clippy, Rust tests, oracle tests, submission verification,
  and diff hygiene pass.
- The pushed PR body contains the new benchmark section exactly once.

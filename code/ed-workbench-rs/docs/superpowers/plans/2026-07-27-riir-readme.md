# RIIR README Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the repository README as a high-energy RIIR project front page without weakening its scientific precision or reproducibility.

**Architecture:** Keep the README as the single public entry point, but reorganize it into a manifesto-style opening, a compact evidence scoreboard, a clear Rust-versus-oracle boundary, and a Level 0-4 progression. Preserve the existing reports and fixtures as the authoritative detail layer.

**Tech Stack:** GitHub-flavored Markdown, Rust/Cargo command examples, existing repository verification scripts.

## Global Constraints

- Preserve every existing energy, error, residual, tolerance, geometry, unit, paper-anchor count, and runnable command.
- Do not claim performance numbers that were not measured.
- Do not conflate primary H2O/6-31G results with Kállay 2001 DZ/DZP targets.
- Clearly identify PySCF as an independent oracle/fixture generator rather than a production runtime dependency.
- Keep all local documentation and report links valid.

---

### Task 1: Rewrite the README Front Page

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: verified numerical evidence under `reports/`, committed fixtures under `fixtures/`, and the public challenge/release links.
- Produces: the repository's public GitHub landing page and command-level entry point.

- [x] **Step 1: Capture the current numerical and command anchors**

Run:

```bash
git show HEAD~1:README.md | rg '245,025|CC\\(8\\)|CI\\(8\\)|MBPT\\(20\\)|hartree|cargo run|RAYON_NUM_THREADS|scripts/verify-submission'
```

Expected: the complete set of headline values and command lines from the pre-redesign README.

- [x] **Step 2: Rewrite the opening and navigation**

Replace the generic title and status introduction with:

```markdown
# Rewrite It In Rust! — Electronic Structure All the Way to CC(8)

**245,025 determinants. Arbitrary-order CC. Direct integrals. One Rust
workbench. RIIR!**
```

Follow it with a results table, `This Is Not a Wrapper`, and a compact table
of contents.

- [x] **Step 3: Reframe Levels 0-4 as a capability climb**

Use Level headings that state the accomplishment:

```markdown
## The Climb
### Level 0 — Make the Tiny Systems Tell the Truth
### Level 1 — Stop Building the Matrix
### Level 2 — Climb from CC(1) to CC(8)
### Level 3 — Keep Going: CI, MBPT, and UCC
### Level 4 — Remove Python from the Production Path
```

Keep each level's exact evidence, commands, and report link.

- [x] **Step 4: Preserve limitations and scientific qualifiers**

Retain an explicit scope boundary stating that Kállay 2001 DZ/DZP calculations
are extended targets and that no 6-31G result is evidence for a different
Hamiltonian.

- [x] **Step 5: Review the resulting Markdown diff**

Run:

```bash
git diff -- README.md
git diff --check
```

Expected: an energetic documentation-only rewrite with no whitespace errors.

### Task 2: Validate and Commit the Redesign

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-07-27-riir-readme.md`

**Interfaces:**
- Consumes: the rewritten README from Task 1.
- Produces: a verified commit suitable for pushing to the public repository.

- [x] **Step 1: Check all relative Markdown links**

Run a local link scan that extracts every non-HTTP Markdown target from
`README.md` and verifies that each target exists.

Expected: no missing repository-relative targets.

- [x] **Step 2: Run the submission gate**

Run:

```bash
scripts/verify-submission.sh
```

Expected: formatting, Clippy, locked tests, tracked JSON, checksums, Python
tests, geometry tests, and diff hygiene all pass.

- [x] **Step 3: Inspect the final repository state**

Run:

```bash
git diff --stat
git status --short
```

Expected: only the README and this implementation plan are pending.

- [x] **Step 4: Commit the completed README**

Run:

```bash
git add README.md docs/superpowers/plans/2026-07-27-riir-readme.md
git commit -m "docs: bring RIIR energy to the README"
```

Expected: one documentation commit containing the redesign and its execution
plan.

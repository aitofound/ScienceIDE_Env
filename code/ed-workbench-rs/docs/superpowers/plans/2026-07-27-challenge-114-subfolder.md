# Challenge 114 Subfolder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a `challenge-114/` workspace that tracks tenferro-rs verification work derived from #129 ED/FCI workloads.

**Architecture:** The #114 material lives as documentation-first scaffolding under `challenge-114/`. Workload specs reference #129 concepts but do not import code yet, so the scaffold remains stable while #129 implementation evolves.

**Tech Stack:** Markdown, JSON, Rust/Cargo repository conventions, GitHub issue and PR links, future benchmark backends including tenferro-rs, PyTorch, and JAX.

## Global Constraints

- Do not modify existing #129 Rust implementation files for this scaffold.
- Use `challenge-114/` as the root for #114-specific material.
- Keep empty conceptual directories trackable with README files.
- Use `profiles/apple-silicon.json` as the initial hardware profile template.

---

### Task 1: Add Challenge 114 Workspace Documentation

**Files:**
- Create: `challenge-114/README.md`
- Create: `challenge-114/docs/challenge-114-brief.md`
- Create: `challenge-114/docs/benchmark-plan.md`
- Create: `challenge-114/docs/gap-log.md`
- Create: `challenge-114/docs/upstream-repos.md`

**Interfaces:**
- Consumes: #114 issue requirements and #129 workload names.
- Produces: stable documentation entry points for future benchmark and oracle work.

- [ ] **Step 1: Create the README**

Write `challenge-114/README.md` with links to #114, #129, tenferro-rs, tenferro-benchmark, and tensor-ad-oracles. State that this workspace derives verification cases from #129 workloads.

- [ ] **Step 2: Create the brief**

Write `challenge-114/docs/challenge-114-brief.md` with metadata, objective, success criteria, and scoped first targets.

- [ ] **Step 3: Create the benchmark plan**

Write `challenge-114/docs/benchmark-plan.md` with benchmark families for small eager loops and permutation/indexed-access-heavy operations.

- [ ] **Step 4: Create the gap log**

Write `challenge-114/docs/gap-log.md` with a table schema and initial open candidate rows.

- [ ] **Step 5: Create the upstream repo index**

Write `challenge-114/docs/upstream-repos.md` with repository links and intended future PR targets.

### Task 2: Add Workload, Benchmark, Result, and Profile Templates

**Files:**
- Create: `challenge-114/workloads/level0-dense-fci/README.md`
- Create: `challenge-114/workloads/sigma-vector/README.md`
- Create: `challenge-114/workloads/amplitude-updates/README.md`
- Create: `challenge-114/benchmarks/permutation-einsum/README.md`
- Create: `challenge-114/benchmarks/eager-small-loops/README.md`
- Create: `challenge-114/results/README.md`
- Create: `challenge-114/profiles/apple-silicon.json`

**Interfaces:**
- Consumes: documentation from Task 1.
- Produces: trackable placeholders with concrete benchmark scope and hardware metadata fields.

- [ ] **Step 1: Create workload specs**

Add one README for each workload family: Level 0 dense FCI, sigma-vector construction, and CC amplitude updates.

- [ ] **Step 2: Create benchmark family specs**

Add one README for permutation-heavy einsum and one for eager small loops.

- [ ] **Step 3: Create result index**

Add `challenge-114/results/README.md` describing where raw and summarized measurements will go.

- [ ] **Step 4: Create Apple Silicon profile**

Add `challenge-114/profiles/apple-silicon.json` with fields for OS, chip, memory, Rust, Python, JAX, PyTorch, and tenferro-rs versions.

- [ ] **Step 5: Verify and commit**

Run `git diff --check -- docs/plans/2026-07-27-challenge-114-design.md docs/superpowers/plans/2026-07-27-challenge-114-subfolder.md challenge-114`. Stage only these paths and commit with `Add challenge 114 verification workspace`.


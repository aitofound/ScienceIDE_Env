# Challenge #129 Submission Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the completed #129 workbench into a review-ready public `v0.1.0` release with automated verification, accurate GitHub metadata, package/API documentation, and an anonymously reproducible tag.

**Architecture:** A portable fail-fast shell script defines the normal verification contract. GitHub Actions calls the same script for pushes and pull requests, while a manual workflow runs the two long primary published-series calculations. Cargo metadata, crate documentation, repository/PR copy, and a source release make the already validated scientific implementation discoverable and immutable.

**Tech Stack:** Rust 2024, Cargo, Bash, jq, Python 3.12/PySCF 2.14.0, GitHub Actions, GitHub CLI.

## Global Constraints

- Do not change any FCIDUMP byte or existing numerical reference/result field.
- Do not change FCI, CC, CI, MBPT, UCC, RHF, or integral algorithms.
- Normal CI must finish without running the two ignored approximately three-minute primary calculations.
- Long primary calculations must remain available through a manual workflow.
- All action versions must use current official release tags checked on 2026-07-27.
- The repository remains public; do not toggle visibility during the polish pass.
- The `v0.1.0` tag must point at the final workbench commit used by the release.
- Kállay DZ/DZP and stretched-water calculations remain out of scope.

---

### Task 1: Define One Local Verification Command

**Files:**
- Create: `scripts/verify-submission.sh`
- Modify: `README.md`

**Interfaces:**
- Consumes: repository root, `Cargo.lock`, tracked JSON files, fixture `reference.json` files, neighboring FCIDUMP files, and a Python interpreter selected by `PYTHON` or `.venv/bin/python`.
- Produces: exit status zero only when normal Rust, Python, JSON, and checksum gates pass.

- [x] **Step 1: Verify the command is initially absent**

Run:

```bash
test ! -e scripts/verify-submission.sh
```

Expected: exit status zero.

- [x] **Step 2: Create the fail-fast verifier**

Create an executable Bash script with this behavior:

```bash
#!/usr/bin/env bash
set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository_root"

cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test --locked

while IFS= read -r -d '' json_file; do
  jq empty "$json_file"
done < <(git ls-files -z '*.json')

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

while IFS= read -r -d '' reference_file; do
  expected=$(jq -r '.fcidump_sha256 // empty' "$reference_file")
  if [[ -n "$expected" ]]; then
    fcidump="$(dirname "$reference_file")/FCIDUMP"
    [[ -f "$fcidump" ]]
    actual=$(sha256_file "$fcidump")
    [[ "$actual" == "$expected" ]]
  fi
done < <(find fixtures -name reference.json -type f -print0)

python_bin=${PYTHON:-.venv/bin/python}
if [[ ! -x "$python_bin" ]] && ! command -v "$python_bin" >/dev/null 2>&1; then
  printf 'Python oracle environment not found: %s\n' "$python_bin" >&2
  exit 1
fi
"$python_bin" -m unittest scripts.oracle.test_units -v

git diff --check
```

Make it executable:

```bash
chmod +x scripts/verify-submission.sh
```

- [x] **Step 3: Run the verifier locally**

Run:

```bash
scripts/verify-submission.sh
```

Expected: 57 Rust tests and five Python tests pass; two long live-primary Rust tests remain ignored; all JSON and checksums pass.

- [x] **Step 4: Document the single-command path**

Add to the README verification section:

````markdown
Run all normal submission gates with:

```bash
scripts/verify-submission.sh
```

Set `PYTHON=python3` when the pinned oracle dependencies are installed in the
active interpreter rather than `.venv`.
````

- [x] **Step 5: Commit**

```bash
git add scripts/verify-submission.sh README.md
git commit -m "Add one-command submission verification"
```

---

### Task 2: Add Normal and Manual GitHub Actions

**Files:**
- Create: `.github/workflows/ci.yml`
- Create: `.github/workflows/primary-live.yml`

**Interfaces:**
- Consumes: `scripts/verify-submission.sh`, `scripts/oracle/requirements.txt`, committed fixtures, release CLI commands.
- Produces: a normal required-quality signal and a separately dispatchable primary numerical acceptance workflow.

- [x] **Step 1: Confirm no workflow currently exists**

Run:

```bash
test ! -d .github/workflows
```

Expected: exit status zero.

- [x] **Step 2: Add the normal workflow**

Create `.github/workflows/ci.yml` with:

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
  workflow_dispatch:

permissions:
  contents: read

concurrency:
  group: ci-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  verify:
    runs-on: ubuntu-latest
    timeout-minutes: 30
    steps:
      - uses: actions/checkout@v7.0.1
      - uses: Swatinem/rust-cache@v2.9.1
      - uses: actions/setup-python@v7.0.0
        with:
          python-version: "3.12"
          cache: pip
          cache-dependency-path: scripts/oracle/requirements.txt
      - name: Install oracle dependencies
        run: python -m pip install -r scripts/oracle/requirements.txt
      - name: Verify submission
        env:
          PYTHON: python
        run: scripts/verify-submission.sh

  minimum-rust:
    runs-on: ubuntu-latest
    timeout-minutes: 20
    steps:
      - uses: actions/checkout@v7.0.1
      - name: Install Rust 1.89.0
        run: rustup toolchain install 1.89.0 --profile minimal
      - name: Check minimum Rust version
        run: cargo +1.89.0 check --locked
```

- [x] **Step 3: Add the manual primary workflow**

Create `.github/workflows/primary-live.yml` with two independent jobs:

```yaml
name: Primary live acceptance

on:
  workflow_dispatch:
    inputs:
      rayon_threads:
        description: Rayon worker count
        required: true
        default: "4"

permissions:
  contents: read

jobs:
  cc-series:
    runs-on: ubuntu-latest
    timeout-minutes: 30
    steps:
      - uses: actions/checkout@v7.0.1
      - uses: Swatinem/rust-cache@v2.9.1
      - run: cargo build --release --locked
      - name: Verify CC(1)-CC(8)
        env:
          RAYON_NUM_THREADS: ${{ inputs.rayon_threads }}
        run: |
          target/release/ed_workbench_rs cc-series \
            fixtures/h2o-631g-fc/FCIDUMP \
            fixtures/h2o-631g-fc/reference.json \
            --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
            --max-rank 8 --residual-tolerance 1e-6 --max-iterations 100

  level3-series:
    runs-on: ubuntu-latest
    timeout-minutes: 30
    steps:
      - uses: actions/checkout@v7.0.1
      - uses: Swatinem/rust-cache@v2.9.1
      - run: cargo build --release --locked
      - name: Verify CI(1)-CI(8) and MBPT(1)-MBPT(20)
        env:
          RAYON_NUM_THREADS: ${{ inputs.rayon_threads }}
        run: |
          target/release/ed_workbench_rs level3-series \
            fixtures/h2o-631g-fc/FCIDUMP \
            fixtures/h2o-631g-fc/reference.json \
            --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
            --max-ci-rank 8 --max-mbpt-order 20 \
            --ci-residual-tolerance 1e-7 \
            --max-iterations 100 --max-subspace 24
```

- [x] **Step 4: Validate workflow syntax and references**

Use Ruby's bundled YAML parser with aliases enabled and verify every local
path named by the workflows:

```bash
ruby -e 'require "yaml"; ARGV.each { |path| YAML.load_file(path, aliases: true) }' \
  .github/workflows/ci.yml .github/workflows/primary-live.yml
test -x scripts/verify-submission.sh
test -f fixtures/h2o-631g-fc/hirata2000-table2.json
git diff --check
```

Expected: all commands exit zero.

- [x] **Step 5: Commit**

```bash
git add .github/workflows/ci.yml .github/workflows/primary-live.yml
git commit -m "Add automated and live numerical CI"
```

---

### Task 3: Complete Cargo and Public API Metadata

**Files:**
- Modify: `Cargo.toml`
- Modify: `src/lib.rs`
- Modify: `README.md`

**Interfaces:**
- Consumes: existing public modules and AGPL-3.0 license.
- Produces: complete Cargo package metadata, an explicit Rust 1.89 minimum,
  crate-level architecture/unit documentation, and CI/release badges.

- [x] **Step 1: Record the current metadata warning and absent crate docs**

Run:

```bash
cargo package --allow-dirty --no-verify --list 2>&1 | \
  grep 'manifest has no description'
test "$(rg -c '^//!' src/lib.rs || true)" = "0"
```

Expected: both commands exit zero.

- [x] **Step 2: Add package metadata**

Add under `[package]`:

```toml
description = "Determinant-based Rust FCI/ED and arbitrary-order electronic-structure method workbench"
license = "AGPL-3.0-only"
repository = "https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust"
homepage = "https://github.com/QuantumBFS/quantum.harness/issues/129"
readme = "README.md"
rust-version = "1.89"
keywords = ["quantum-chemistry", "fci", "coupled-cluster", "rust", "electronic-structure"]
categories = ["science", "algorithms"]
```

- [x] **Step 3: Add crate-level documentation**

Prepend `src/lib.rs` with crate docs that state:

- the crate is a transparent determinant-based reference workbench;
- `fcidump`, `determinant`, `strings`, `direct_fci`, and `davidson` form the
  FCI path;
- `cluster` and `coupled_cluster` form arbitrary-order CC;
- `truncated_ci`, `mbpt`, and `unitary_cc` are Level 3;
- `libcint_frontend`, `rhf`, and `ao2mo` are the direct-integral path;
- coordinates are Angstrom at public molecule inputs, internally converted to
  Bohr by libcint, energies are Hartree, and wavefunction quantities are
  dimensionless;
- committed PySCF data is oracle-only.

- [x] **Step 4: Add README badges**

Place these directly below the title:

```markdown
[![CI](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/actions/workflows/ci.yml/badge.svg)](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust)](https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust/releases/tag/v0.1.0)
[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
```

- [x] **Step 5: Validate metadata, MSRV, and docs**

Run:

```bash
if cargo package --allow-dirty --no-verify --list 2>&1 | \
    grep -q 'manifest has no description'; then
  exit 1
fi
rustup toolchain install 1.89.0 --profile minimal
cargo +1.89.0 check --locked
RUSTDOCFLAGS='-D warnings' cargo doc --no-deps --locked
cargo test --locked
```

Expected: all commands pass.

- [x] **Step 6: Commit**

```bash
git add Cargo.toml src/lib.rs README.md
git commit -m "Document and package the Rust workbench"
```

---

### Task 4: Create Review and Release Copy

**Files:**
- Create: `docs/submission-pr-body.md`
- Create: `docs/release-notes-v0.1.0.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: final numerical reports, public repository URL, solution PR URL, and reproduction prompt.
- Produces: version-controlled source text for GitHub PR metadata and the `v0.1.0` release.

- [x] **Step 1: Add the PR body source**

Write `docs/submission-pr-body.md` with:

- team and issue identity;
- a statement that the repository is public and the submission is complete;
- Level 0-4 design summary;
- primary FCI energy and residual;
- the 8 CC plus 28 CI/MBPT Table 2 matches;
- links to the public workbench, `v0.1.0`, reports, and reproduction prompt;
- tenferro gap summary;
- explicit Kállay DZ/DZP extended-target boundary;
- reviewer checklist for design README, reproduction prompt, CI, and numerical
  evidence.

- [x] **Step 2: Add release notes**

Write `docs/release-notes-v0.1.0.md` with:

- immutable input SHA-256;
- FCI, CC(2), CC(8), and CI(8) headline values;
- complete method/order coverage;
- toolchain and hardware provenance;
- normal and live verification commands;
- known scope boundary;
- links to detailed reports and upstream PR #217.

- [x] **Step 3: Link release notes from the README map**

Add both new documents to `Repository Map`, describing one as the source for
the upstream PR description and the other as the immutable `v0.1.0` release
record.

- [x] **Step 4: Validate copy against machine data**

Run searches that require all exact anchors:

```bash
rg -q -- '-76\\.1211742041419' docs/submission-pr-body.md docs/release-notes-v0.1.0.md
rg -q '826dd373a8b6047dff8136168431a803b59d9ef029a074da3b8f74f22603db3e' \
  docs/release-notes-v0.1.0.md
rg -q 'CC\\(1\\).*CC\\(8\\)' docs/submission-pr-body.md docs/release-notes-v0.1.0.md
rg -q 'MBPT\\(1\\).*MBPT\\(20\\)' docs/submission-pr-body.md docs/release-notes-v0.1.0.md
git diff --check
```

- [x] **Step 5: Commit**

```bash
git add docs/submission-pr-body.md docs/release-notes-v0.1.0.md README.md
git commit -m "Add review and release documentation"
```

---

### Task 5: Push and Validate GitHub Automation

**Files:**
- Modify: `docs/superpowers/plans/2026-07-27-submission-polish.md`

**Interfaces:**
- Consumes: all local polish commits and public GitHub repository.
- Produces: green remote normal CI on the exact final pre-release commit.

- [x] **Step 1: Run all local gates**

```bash
scripts/verify-submission.sh
RUSTDOCFLAGS='-D warnings' cargo doc --no-deps --locked
git diff --check
git status --short
```

Expected: all pass and only the plan checkbox update remains.

- [x] **Step 2: Mark Tasks 1-4 complete and commit the plan record**

Update completed checkboxes, then:

```bash
git add docs/superpowers/plans/2026-07-27-submission-polish.md
git commit -m "Record submission polish completion"
```

- [x] **Step 3: Synchronize and push**

```bash
git fetch origin
git rebase origin/main
git push origin main
```

Expected: no force push; local `HEAD` equals `origin/main`.

- [x] **Step 4: Wait for normal CI**

Use:

```bash
gh run list --repo JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust \
  --workflow CI --limit 1
gh run watch --repo JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust \
  "$(gh run list --repo JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust \
      --workflow CI --limit 1 --json databaseId --jq '.[0].databaseId')" \
  --exit-status
```

Expected: both `verify` and `minimum-rust` jobs succeed on the pushed commit.

---

### Task 6: Publish v0.1.0 and Finalize Upstream Review

**Files:**
- Modify in upstream PR branch:
  `tracks/ed/solutions/WangTheoPhys/README.md`
- Modify in upstream PR branch:
  `tracks/ed/solutions/WangTheoPhys/reproduction-prompt.md`

**Interfaces:**
- Consumes: green final workbench commit, release notes, PR body source.
- Produces: public tag/release, accurate repository/PR metadata, review request, and anonymous end-to-end proof.

- [x] **Step 1: Update repository metadata**

Run:

```bash
gh repo edit JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust \
  --description "Rust determinant-based FCI/ED, arbitrary-order CC(n), CI(n), MBPT(n), UCC, and direct-libcint workbench for Quantum Harness #129" \
  --homepage "https://github.com/QuantumBFS/quantum.harness/issues/129"
```

Verify visibility remains public.

- [x] **Step 2: Create and push the immutable release**

Require that `v0.1.0` does not already exist, then:

```bash
git tag -a v0.1.0 -m "Quantum Harness #129 validated submission"
git push origin v0.1.0
gh release create v0.1.0 \
  --repo JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust \
  --title "v0.1.0 — Quantum Harness #129 validated submission" \
  --notes-file docs/release-notes-v0.1.0.md \
  --verify-tag
```

- [x] **Step 3: Update upstream solution references**

In a clean temporary clone of `JunkaiWang-TheoPhy/quantum.harness`, check out
`challenge/ed-wangtheophys-rust-workbench`. Replace the validated workbench
revision with the exact tagged commit and add the public `v0.1.0` release
link in both solution files. Modify no other path, commit:

```bash
git commit -m "Link WangTheoPhys v0.1.0 release"
git push origin challenge/ed-wangtheophys-rust-workbench
```

- [x] **Step 4: Replace PR #217's stale body**

Run:

```bash
gh pr edit 217 --repo QuantumBFS/quantum.harness \
  --body-file docs/submission-pr-body.md
```

Then add one review-ready comment mentioning `@chenpeizhi`, the green CI run,
the public `v0.1.0` release, and the two upstream solution files. Do not post
duplicate comments.

- [x] **Step 5: Verify anonymous release reproduction**

Clone without credentials into a fresh temporary directory, check out
`v0.1.0`, and run:

```bash
PYTHON=python scripts/verify-submission.sh
```

after installing `scripts/oracle/requirements.txt` into a fresh Python 3.12
environment. Require:

- anonymous repository and release URLs return HTTP `200`;
- cloned `HEAD` equals the `v0.1.0` peeled tag commit and workbench
  `origin/main`;
- all normal verification gates pass;
- primary FCIDUMP SHA-256 matches;
- PR #217 is open and mergeable;
- both upstream solution files contain the tag and exact workbench commit;
- repository description no longer says “Private workspace”.

- [x] **Step 6: Complete the external-state audit**

Require every preceding Task 6 check to pass and record the immutable evidence
in the final handoff: workbench commit, peeled tag commit, release URL, CI run
URL, PR head, PR state, anonymous clone path, and FCIDUMP SHA-256. Because the
release tag must point to the final source commit, do not create another
workbench commit after tagging.

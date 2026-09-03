# Challenge #129 Submission Polish Design

Date: 2026-07-27

## Objective

Turn the completed and numerically validated #129 workbench into a
review-ready public software submission. The work must improve discoverability,
continuous verification, package metadata, API orientation, and release
reproducibility without changing the validated scientific algorithms or
oracle values.

## Scope

This pass includes:

- replacing stale “private/development” GitHub metadata with the final public
  status and result summary;
- adding a Linux GitHub Actions workflow for formatting, Clippy, locked Rust
  tests, Python unit/geometry tests, tracked-JSON validation, and FCIDUMP
  checksum validation;
- adding an explicit manual workflow for the long primary CC and CI/MBPT
  acceptance calculations rather than running them on every commit;
- adding Cargo package metadata, a tested minimum Rust version, crate-level
  documentation, and README status badges;
- providing one checked script that runs the submission's normal local gates;
- tagging the audited revision and publishing a GitHub `v0.1.0` release with
  the primary numerical summary and reproduction links;
- updating upstream PR #217 to describe the completed public submission and
  requesting review.

This pass does not include the Kállay 2001 DZ/DZP systems, stretched-water
hard mode, analytical UCC gradients, or multi-root Davidson. Those are
separate research extensions with their own scientific inputs and acceptance
criteria.

## Approaches Considered

### 1. Metadata-only cleanup

Update the PR body and repository description, then request review. This is
fast but leaves the public repository without automated evidence and makes
future regressions easy.

### 2. Review-ready release engineering — selected

Add lightweight CI, package/API metadata, a local verification entry point,
and an immutable release in addition to the metadata cleanup. Normal CI uses
only the fast suite; the two approximately three-minute primary calculations
remain explicit manual jobs. This provides strong evidence without wasting
runner time on every documentation commit.

### 3. Full research-extension pass

Include DZ/DZP, stretched geometries, multi-platform live numerical series,
and new method development. This would be valuable future work, but it is not
a submission-polish task and would delay review behind new scientific risk.

## Architecture

`.github/workflows/ci.yml` is the normal gate. It builds the locked Rust
dependency graph on Ubuntu, checks formatting and Clippy, runs all non-ignored
tests, validates every tracked JSON document, verifies every FCIDUMP against
the SHA-256 recorded in its neighboring `reference.json`, and runs the Python
geometry/unit suite in the pinned oracle environment.

`.github/workflows/primary-live.yml` is manual-only. It builds release mode
and invokes the two ignored primary integration tests separately with
`RAYON_NUM_THREADS` configurable as a workflow input. A failure therefore
identifies CC or CI/MBPT independently.

`scripts/verify-submission.sh` mirrors the normal CI contract locally and
fails at the first inconsistent command, JSON document, or checksum.
`Cargo.toml`, `src/lib.rs`, and `README.md` supply package metadata, top-level
API orientation, CI/release badges, and the single-command verification path.

The GitHub release points to the final audited commit and does not distribute
platform-specific binaries: direct `libcint` static builds are platform
sensitive, while source plus `Cargo.lock`, checksummed fixtures, and committed
result records are already the portable artifact.

## Error Handling and Safety

- CI and the local verifier run with fail-fast shell settings.
- Checksum verification discovers reference files rather than hard-coding the
  fixture list and rejects a missing FCIDUMP.
- Normal CI never mutates or regenerates committed oracle fixtures.
- Long live workflows are manual and report CC and Level 3 failures as
  separate jobs.
- The release is created only after the final commit is pushed and the public
  source URL, tag, workflow files, and PR description are re-read from GitHub.
- Existing numerical JSON and FCIDUMP bytes remain immutable.

## Acceptance

The polish pass is complete when:

- the public repository description and PR #217 body describe the completed
  submission rather than a private workspace;
- the normal GitHub Actions workflow is present and green;
- the manual primary workflow is dispatchable;
- `cargo package --allow-dirty --no-verify --list` no longer emits missing
  package-metadata warnings;
- crate documentation builds with warnings denied and provides module-level
  orientation;
- `scripts/verify-submission.sh` passes from the repository root;
- tag and GitHub release `v0.1.0` resolve to the final audited commit;
- PR #217 is open, mergeable, references the release, and has a review
  request or explicit review-ready comment;
- an anonymous clone of `v0.1.0` passes the normal verification script.

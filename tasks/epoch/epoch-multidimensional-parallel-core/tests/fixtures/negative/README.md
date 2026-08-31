# Preserved negative execution-contract fixtures

These fixtures are **not** active checks: they are not listed in
`tests/contract.json` and `tests/harness.py` never loads them. They exist to
demonstrate, and let a maintainer re-demonstrate, that the shared verifier
library (`tests/lib/manifest.py`, `tests/lib/checks.py`) fails closed on the
execution-contract violations the frozen task contract calls out explicitly:
rank duplication, rank loss, seam discontinuity, and aliased/contained output
roots. They are preserved evidence from local validation (no Docker/EPOCH
build/MPI/network was used to produce them; every block below is a synthetic
JSON fixture standing in for a real SDF/execution.json pair).

Run `python3 build_and_check.py` from this directory to regenerate the
fixtures under a freshly created, uniquely named scratch directory
(`tempfile.mkdtemp`, printed to stdout as the "retained scratch root") and
re-print the pass/fail verdict for each. Expected output: every scenario
below is rejected (`passed=False` or `authenticated=False`), never silently
accepted. The script never deletes, moves, or truncates that directory or
any file in it -- every fixture tree it writes is left on disk after the
script exits, for later inspection.

## Scenarios

1. **wrong-decomposition** (seam discontinuity proxy) -- a `cpu_rank` block
   claims the (2,3) rank-grid factorization for `decomp-2d-auto-rank6`
   instead of the (3,2) split_domain's own area-minimizing search actually
   selects for that grid. Rejected by `lib/checks.py::_auto_decomposition`.
2. **rank-count-mismatch** (rank duplication/loss proxy) -- `execution.json`
   declares `rank_count=4` and an `mpirun -n 4` argv while the check demands
   8 ranks. Rejected by `lib/manifest.py::verify_rank_and_argv` before any
   physics is even inspected (authentication failure, not a scoring failure).
3. **tampered-digest** -- `execution.json` declares a `stdout_sha256` that
   does not match the actual `stdout.log` bytes on disk. Rejected by
   `lib/manifest.py::authenticate_execution`, which always recomputes every
   digest from the raw file rather than trusting the declared value.
4. **aliased-roots** -- reference and candidate point at the same directory,
   one containing the other, and a set of files that share an inode between
   the two roots. All three are rejected by
   `lib/manifest.py::non_alias_audit`, which the harness runs before scoring
   whenever `EPOCH_MDPC_SELF_TEST=1`.

## Manifest source-join regression (separate script)

`manifest_source_join_check.py` is a separate, narrower static-plus-real-data
check: it proves `solution/solve.sh`'s container-side pinned-source
verification joins the shared source root as `source / entry["path"]`
(the fixed, correct contract) rather than the earlier `source.parent /
entry["path"]` (which resolved one directory too high and reported every
pinned file missing before any EPOCH build/MPI/physics ran). It uses the
real `comment/source-manifest.json` and the real repository-shared
`code/epoch` tree -- confirming every one of the 27 pinned entries resolves
and hashes correctly under the fixed join and is wrong/missing under the old
one -- then statically confirms `solve.sh` itself uses the fixed join and
that a synthetic old-parent-join or other mismatched-join heredoc is
rejected by the same check. Run `python3 manifest_source_join_check.py` from
this directory; it also writes its (retained, never-cleaned-up) synthetic
heredoc copies under a fresh `tempfile.mkdtemp` scratch root printed to
stdout.

## SDF sibling build-closure regression (separate script)

`sdf_sibling_closure_check.py` is a separate, narrower static-plus-real-data
check: it proves `solution/solve.sh`'s temp-build step creates a fresh
dimension-specific parent directory under `BUILDROOT` for each of
epoch1d/epoch2d/epoch3d and copies both `$SOURCE/$dim` and `$SOURCE/SDF`
into that same fresh parent -- so each dimension's own `SDF := ../SDF/FORTRAN`
Makefile declaration resolves correctly and no SDF build state is shared or
mutated across dimensions -- rather than the earlier defect, which copied
only `$SOURCE/$dim` and left the required sibling SDF tree absent (`make
../SDF/FORTRAN: No such file or directory`). It uses the real
`code/epoch/epoch{1,2,3}d/Makefile` files and the real, repository-shared
`code/epoch/SDF/FORTRAN` tree, both statically (classifying `solve.sh`'s
build-loop text) and by actually copying the real trees on disk and
confirming `../SDF/FORTRAN` does or does not resolve to a real directory
under the fixed, old (missing-SDF), and a wrong-placement (shared/mistargeted
SDF) topology. Run `python3 sdf_sibling_closure_check.py` from this
directory; it also writes its (retained, never-cleaned-up) synthetic
build-loop snippets and on-disk topology trees under a fresh
`tempfile.mkdtemp` scratch root printed to stdout.

## Run-plan stdin isolation and execution-completeness regression (separate script)

`runplan_stdin_completeness_check.py` is a separate, narrower static-plus-
real-data check for schema v3 (the "stdin" execution-manifest field, added
alongside `solution/solve.sh`'s planned-vs-completed execution counter and
post-loop result-tree audit). It proves, using real (not mocked) subprocess
and shell execution wherever possible:

1. **The mechanism is real.** A real bash `while ... done < run-plan.tsv`
   loop, launching a real (non-MPI, non-EPOCH) child process via the exact
   `subprocess.run(argv, ..., stdout=out, stderr=err, ...)` call shape
   `solution/run_epoch.py` uses, completes only 1 of 5 planned rows when the
   child's stdin is left unbound (the pre-repair defect: the child inherits
   the loop's own run-plan stdin and consumes it), and completes all 5 when
   the child's stdin is explicitly bound to `subprocess.DEVNULL` (the fix).
2. **The real fix is shipped.** The real, current
   `solution/run_epoch.py` text is confirmed to call
   `subprocess.run(argv, ..., stdin=subprocess.DEVNULL, ...)`; a
   reconstruction of the old call (no `stdin=` kwarg) is rejected by the
   same classifier.
3. **The completeness audit is real and works.** `solution/solve.sh`'s own
   shipped `COMPLETENESS_AUDIT_PY` heredoc (and its run-plan-generation
   heredoc) are extracted verbatim -- never reimplemented -- and executed
   against seven synthetic adversarial result trees (missing execution,
   wrong label/folder, extra/unplanned execution, duplicate plan entry,
   mismatched check_id, missing check id, and the accepted happy path) and
   against the real `tests/contract.json` (28 planned executions across 19
   checks): the actual preserved compatibility-C evidence shape -- only
   `decomp-1d-auto-rank4/auto/execution.json` present -- is confirmed
   rejected, and the real 28-of-28/19-of-19 happy path is confirmed
   accepted with the real `executions=`/`checks=` counts in its output.

A manifest with a missing, wrong, or non-string `stdin` field is a
*manifest-authentication* concern, not a run-plan-completeness one, and is
covered by `build_and_check.py`'s `missing-stdin-field`,
`wrong-stdin-value`, `non-string-stdin-value`, and
`old-24-key-manifest-rejected` scenarios instead. Run
`python3 runplan_stdin_completeness_check.py` from this directory; it also
writes its (retained, never-cleaned-up) synthetic runners, TSVs, and result
trees under a fresh `tempfile.mkdtemp` scratch root printed to stdout.

## Schema v4 relocation-neutral `cwd` provenance

Since schema v4, `execution.json`'s `"cwd"` field records a task-root-relative
`"<check_folder>/<run_label>"` POSIX path rather than the producer's own
absolute filesystem location (e.g. the oracle container's
`/app/results/<folder>/<label>` mount point), so the same evidence tree
authenticates identically whichever absolute root a verifier later mounts it
under (`/reference/...`, `/candidate/...`, or anything else). `build_and_check.py`'s
`cwd-old-v3-absolute-shape-rejected`, `cwd-absolute-candidate-root-rejected`,
`cwd-absolute-reference-root-rejected`, `cwd-absolute-tmp-root-rejected`,
`cwd-leading-traversal-rejected`, `cwd-embedded-traversal-rejected`,
`cwd-wrong-folder-rejected`, `cwd-wrong-label-rejected`,
`cwd-extra-path-component-rejected`, `cwd-backslash-path-rejected`,
`cwd-non-string-rejected`, and `cwd-missing-field-rejected` scenarios prove
`lib/manifest.py::authenticate_execution` rejects every non-conforming shape,
while `cwd-valid-relative-accepted` (in `POSITIVE_SCENARIOS`) proves the one
valid relative form still authenticates cleanly. `copied-content-in-distinct-tree`
was updated to match: since an identical `check_folder/run_label` subtree may
now legitimately live under any root, it instead proves genuine bytes copied
into a directory whose own name disagrees with its declared `run_label` are
still caught by the independent structural check against the actual run
directory's own path components.

## Packaging-contract regression (separate script)

`dockerfile_manifest_packaging_check.py` is a separate, narrower static
check: it proves `tests/Dockerfile`'s `COPY comment/source-manifest.json ...`
line lands the file at the exact path `solution/solve.sh`'s container-side
body reads it from (`/app/comment/source-manifest.json`), and that a
synthetic Dockerfile missing that `COPY` or copying it to the wrong
destination is caught. Run `python3 dockerfile_manifest_packaging_check.py`
from this directory; it also writes its (retained, never-cleaned-up)
synthetic Dockerfiles under a fresh `tempfile.mkdtemp` scratch root printed
to stdout.

## Self-contained SDF reader regression (two separate scripts)

`tests/lib/sdf_read.py` no longer imports the external `sdf`/`sdf_helper`
C-extension (built from `code/epoch/SDF/utilities`, not installed in the
declared verifier environment); it decodes EPOCH's `.sdf` binary format
itself, in pure Python + numpy, straight from the file-format spec
cross-checked against the pinned C reader and Fortran writer (see that
module's own docstring for exact source citations, including the two
different cpu-split encodings it supports: geometry 1, `cpu_rank`'s
per-axis ladder with the on-disk array one entry short per axis, which
`cpu_split_boundaries` reconstructs; and geometry 4, `cpu/<species>`'s full,
un-elided, direct per-rank particle-count array, which `species_cpu_split`
decodes as-is). `sdf_synth_fixture.py` is a shared, non-reward-path helper
the scripts below import: it builds small, well-formed `.sdf` byte strings
from first principles (independent of `sdf_read.py`'s own decoder)
describing one coherent synthetic 2-D EPOCH dump (grid, an `ex` field, a
geometry-1 `cpu_rank` ladder, a geometry-4 `cpu/tracer` species split, and
that species' particle positions), and exports the values a correct read
must reconstruct, plus `patch_geometry`/`patch_cpu_split_values` helpers for
targeted cpu-split corruption.

1. `sdf_reader_synthetic_check.py` proves the reader itself is correct: every
   grid/field/particle/scalar block kind these checks consume round-trips
   correctly (including field staggering metadata, the geometry-1 ladder
   reconstruction, and the geometry-4 direct-count decode), the reader
   fails closed (raises `sdf_read.SdfFormatError`, never silently wrong
   data) on a truncated payload, a declared-size mismatch, a cross-block
   overread, and -- specifically for geometry-4 `cpu/<species>` blocks --
   a truncated payload, a per-rank-count sum that no longer matches the
   species' recorded total, a negative per-rank count, either cpu-split
   quantity claiming the other's geometry, and an entirely unsupported
   geometry value; the shipped `tests/lib/sdf_read.py` source also contains
   no `import sdf` statement anywhere.
2. `sdf_reader_import_smoke.py` proves the fix actually closes the reported
   defect end to end: with `sys.modules['sdf']` forced to `None` (so any
   `import sdf` anywhere raises `ImportError`, exactly as if the package
   were absent from the verifier image), it authenticates and evaluates one
   hand-authored `partition-coverage` check -- not one of
   `tests/contract.json`'s 19 real rows, and this script makes no claim
   about those -- against a synthetic run tree (one synthetic `.sdf` file
   plus a fully-authenticating `execution.json`, reusing the real
   `decomp-2d-auto-rank6/deck/input.deck` fixture's bytes for the deck
   digest), and confirms `tests/lib/checks.py -> tests/lib/manifest.py ->
   tests/lib/sdf_read.py` imports and reaches task-owned SDF parsing (rather
   than raising `ModuleNotFoundError: No module named 'sdf'`) all the way to
   a passing verdict.
3. `repair_regression_check.py` covers the 2026-08-30 real-SDF
   self-validation repair's other four failure classes end to end, each
   with a positive case plus an adversarial mutant that must still be
   rejected: `decomposition.uneven_split`'s remainder placement and
   `decomposition.auto_split_2d`/`auto_split_3d`'s particle-free
   `get_optimal_layout` search (both unit-level, against real pinned-EPOCH
   dump values, with the previous, now-wrong implementations reproduced
   inline as named mutants); `tests/contract.json`'s PAR-12
   `dlb-conservation` params (full `checks.evaluate()` round trip: the
   previous `{"quantity": "particle_count"}` params reproduce the
   `KeyError('species')` defect, the repaired `{"species": "electron"}`
   params pass a genuine rebalance and still reject a non-conserving
   mutant); and PAR-17's statistical-budget tolerance in
   `checks.py::_global_scalar_equivalence` (full `checks.evaluate()` round
   trip: the initial-condition snapshot keeps its original strict
   tolerance, a post-integration difference within the documented
   1/sqrt(N) PIC shot-noise budget passes, and one far beyond it still
   fails).

Run `python3 sdf_reader_synthetic_check.py`, `python3
sdf_reader_import_smoke.py`, and `python3 repair_regression_check.py` from
this directory; all three write their (retained, never-cleaned-up)
synthetic `.sdf` files and run trees under a fresh `tempfile.mkdtemp`
scratch root printed to stdout.

## Repair2 (2026-08-30) regression: PAR-12/13/15/18

`repair2_regression_check.py` covers the second, exact-G/H-reader3-sourced
self-validation repair's four failure classes, each with a positive case
plus at least one adversarial mutant that must still be rejected, using
real, hand-built `.sdf` files (via `sdf_synth_fixture.py`) and the real
`tests/lib/checks.py` invariant functions (only `_authenticate_all` is
stubbed out, to isolate the invariant math from the separately-covered
manifest-authentication layer):

1. **PAR-12** (`_dlb_conservation`): a pre-loop rebalance whose ladder
   already differs from `decomposition.uneven_split`'s naive baseline at
   every snapshot (and never changes again) passes; a ladder that matches
   the naive baseline at every snapshot and never changes -- the real,
   unrepaired G/H defect -- is still rejected.
2. **PAR-13** (`_dlb_field_integrity`): a pre-loop-only redistribution
   (non-naive, unchanging ladder) with a finite, non-negative energy series
   passes; no redistribution at all (naive, unchanging ladder) still
   authfails; a genuine within-run (dump-bracketed) redistribution
   transition within the margin over the run's own non-redistribution
   deltas passes, the same transition blown up far beyond that margin still
   fails, and a negative energy value is still rejected.
3. **PAR-15** (`_flat_rank_to_axis_indices`): particles placed per the
   correct (x-fastest, per `mpi_routines.F90`'s `setup_communicator`)
   rank-to-domain mapping pass; the same particles placed per the previous
   (z-fastest) mapping -- reproduced inline as a named mutant -- are now
   rejected, as is an axis-order-independent genuine ownership violation
   (two ranks' particle blocks swapped).
4. **PAR-18** (`initial-condition-equivalence` params): the repaired
   `["ex", "bz"]` field list passes on matching data and still rejects a
   genuine `ex` mismatch; the previous `["ex", "ey", "bz"]` params reproduce
   the real `KeyError`-driven authfail against data that (like the real
   deck) never has an `ey` block.

Run `python3 repair2_regression_check.py` from this directory; it also
writes its (retained, never-cleaned-up) synthetic `.sdf` files under a fresh
`tempfile.mkdtemp` scratch root printed to stdout.

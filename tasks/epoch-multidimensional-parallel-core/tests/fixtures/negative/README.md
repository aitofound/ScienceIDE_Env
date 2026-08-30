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

# tgaqd

Upstream test: `code/spherepack/test/tgaqd.f` (built and run by `make` in `test/`). Policy: `invariants`.

## The test

`run.sh` builds `libspherepack.a` from `code/spherepack/src` (gfortran `-fdefault-real-8 -O2 -std=legacy`,
shared across every check of the same run; `-O0` for `altbuild`), compiles `ic/<initial condition>/tgaqd.f`
against it, and captures the driver's own stdout to `stdout.log`. `ic/nominal/tgaqd.f` is byte-identical to
the upstream test; it exercises gaqd Gauss-Legendre points and weights, single vs double precision reference formulas. Native runtime on 2026-09-16 was 0.03 s; well
under 300 s including the build. `frames: not applicable, one solve` -- this is a one-shot Fortran 77 program,
not a time-stepping code, so the five-frame rule does not apply. The only knob is `SAB_CPUS` (the driver is
single-threaded with a compile-time grid size, so there is nothing else to scale); `run.sh --help` lists it.

## The two initial conditions

`ic/nominal/tgaqd.f` is the unmodified official test source. `ic/variant/tgaqd.f` changes exactly one line:
both of the driver's own `pis2` derivations (4*atan(1) and 2*atan(1), each feeding a different reference computation of pi in the file) is multiplied by `1.0000000000000004D0` (a 2-ulp perturbation at binary64, about a
4.4e-16 relative change). `run.sh altbuild` reruns `ic/nominal/tgaqd.f` on the alternative build: the same
pinned source compiled with `-O0` instead of `-O2` (same-compiler fallback; `-O2` is already IEEE with no
fast-math, so there is no strict-IEEE flip to declare for this codebase family).

## The pass policy

Every integer the driver prints (grid sizes, `icase`/`ierror` control-flow flags, work-space lengths) must
match the reference exactly; a mismatch means a dropped test case or a wrong error code. Every floating value
the driver prints is compared under one absolute bound of `1e-8` (no relative term): SPHEREPACK's test fields
are band-limited functions the transforms under test represent exactly, so every printed "error" is round-off
from the forward/inverse chain, measured at 1e-13 to 1e-17 natively (2026-09-16, matching the shipped
`output/darwin.dp` transcript in magnitude, not digit for digit). The 2-ulp variant and the `-O0` altbuild both
move these lines by 1e-15 to 1e-17, several orders of magnitude under the bound, while a real implementation
fault (a wrong stride, a transposed array, a dropped term) raises them past `1e-8` by a wide margin. CPU timings this driver prints (tdoub, tsing, irele) are stripped before comparison: wall-clock bookkeeping, never physics. The argmax index `irele` (the grid point at which the driver's printed maximum error is attained) is also stripped: round-off can move it between two nearly tied cells, so it is bookkeeping, not physics.

## Evidence

The nominal-vs-variant spread measured natively on 2026-09-16 (gfortran 15.2, arm64) and the in-container
spread and altbuild floor from `sab.py task selfcheck` are recorded in `rubric.json` under `evidence`. Nothing
here describes the reference outputs.


## Altbuild finding (2026-09-16)

The altbuild comparison first failed on the worker because this driver's CPU-timing line
(`tdoub gaqd`, an extra word between the label and the number) was not recognised by the
validator's timing-exclusion pattern, so a genuinely faster/slower `-O0` timing leaked into
the graded set. No source change: the validator's exclude-keyword match was widened to skip
any run of non-numeric characters between the label and its number. `code/spherepack/src`
and this check's `ic/` copies are unmodified.

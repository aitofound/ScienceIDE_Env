# tcud3cr

Upstream test: `code/mudpack/test/tcud3cr.f`. Policy: `invariants` (chaotic: true; the leaf's other 35 checks are `pointwise` -- see why below).

**Decision taken on the human's behalf, flagged for review**: this check's policy, graded observables and bound were set by the packager during calibration, not by the human, because the printed default from the skill's own remedy order (shorter window, invariants, loosen last) was unambiguous once the mechanism below was measured. The bound (`atol=10.0` on a scalar) is wider than every other check in this leaf; the reasoning is below and the curator should read it.

## The test

`run.sh` builds the whole MUDPACK static library from the untouched pinned
source and links it against `ic/<nominal|variant>/driver.f`: a copy of the
official test driver `code/mudpack/test/tcud3cr.f` (cud3cr: complex
second-order multigrid solver, 3D nonseparable elliptic PDE with a cross
derivative) with one output block added before the program's final `end`.
The added block writes the solved complex grid array `phi(nx,ny,nz)`, the
driver's own final "exact least squares error" scalar (`err2`), and the
driver's own outer-iteration termination code (`ierror`) to
`sab_solution.dat`. Nothing about the solver call, the PDE coefficients, the
boundary conditions or the grid size is changed from the official test.
`run.sh --help` lists `SAB_CPUS`, informational only. The whole check
(build, compile, solve) measured well under a second natively.

## The two initial conditions

`ic/nominal/driver.f` is the instrumented copy of the official test,
unchanged. `ic/variant/driver.f` differs only in the literal value of `xb`,
changed from `1` to `1.0000000000000004`: exactly two upward binary64 ULPs.
On arm64/gfortran 15.2 this leaves the driver's final `err2` unchanged to 14
significant digits; **on the worker (x86_64) it moved `err2` from
`0.09863272` (nominal) to `2.6234380368819...` (variant) -- a 26x jump from
the smallest representable input change.** This is the direct evidence for
the mechanism below: it is not a broken variant, it is the same signature
as the known pitfall "meep-mpb-eigensolver-two-state" ("the variant's
distance does not scale with the perturbation... a coin flip between
answers"). `run.sh altbuild` runs `ic/nominal` on the same
pinned source built with `-O0` instead of `-O2`; this produced a fourth
distinct value (`3.0563886166771979`, on arm64).

## The pass policy

**This check does not grade the solved array pointwise, unlike this leaf's
other 35 checks.** `test/tcud3cr.f`'s cross-derivative term is corrected by
an outer Picard-style iteration built into the library routine `cud3cr`
itself (`tol=0.0010`, `maxit=10`, both printed by the driver). Measured
across two architectures, two optimization levels and one input
perturbation (five runs total): every run hits `ierror=-10` (the iteration
cap, not a hard error, and not an unexpected clean convergence), and the
driver's own printed "relative difference profile" stays near `1.6`-`1.7`
across all 10 outer iterations every time -- the correction never
approaches its own `0.0010` target, so it is not contracting. Under that
regime, `err2` lands on one of a small set of values depending on the exact
rounding path rather than converging to one answer: measured
`err2` in `{0.09863272, 2.6234380368819084, 2.6234380368819163,
~2.6234380368819, 3.0563886166771979}` across those five runs -- a
top-to-bottom spread of `2.9578`, three to four orders of magnitude above
every other check's measured build floor in this leaf -- with the identical
`ierror=-10` throughout.

Per the remedy order (shorter window, invariants, loosen last): shortening
is unavailable (`maxit=10` is the official test's own fixed configuration,
not a tunable window), so this check grades **two invariants**, both read
from the trailing lines of `solution.dat` (the array values earlier in the
file are still produced, for a solver to read, but are not graded):

1. **`ierror`, exact.** A port that silently converges (`ierror=0`) or traps
   (`ierror>0`) has changed this official test's own documented, shipped
   outcome, regardless of `err2`.
2. **`err2`, absolute band `atol=10.0`, `rtol=0`.** An absolute bound, not a
   relative one, because the reference value has been measured as low as
   `0.0986`, where a relative bound would be far too tight. `10.0` is
   roughly `3.4x` the widest pairwise gap measured across the five
   legitimate runs above (`2.9578`), while a structurally wrong correction
   (a dropped or mis-signed term) would be expected to push the
   least-squares residual formula toward a qualitatively different regime
   (much larger, or non-finite), not a value that happens to land within
   `3x` of the measured legitimate range by chance.

This bound is looser than every other check in this leaf and is revisited
if a wider legitimate spread is measured on a third architecture or build;
see `rubric.json`'s `warrant` for the full argument.

## Evidence

Five measurements before/alongside the worker selfcheck: arm64/gfortran
15.2 `-O2` nominal `err2=2.6234380368819084`; arm64/gfortran 15.2 `-O0`
altbuild `err2=3.0563886166771979`; arm64/gfortran 15.2 `-O2` 2-ULP variant
`err2=2.6234380368819163`; x86_64 (worker) `-O2` nominal
`err2=0.09863272`; x86_64 (worker) `-O2` 2-ULP variant
`err2=2.6234380368819...`. `ierror=-10` in every one. Widest pairwise gap:
`2.9577558966771978`. Numbers are revised once more from the worker's own
final selfcheck record. See `comment/README.md` "## Blind spots" for the
leaf-level summary and the Known-pitfall candidate this mechanism suggests.

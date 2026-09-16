# thwscsp

Upstream test: `code/fishpack/test/thwscsp.f`. Policy: `pointwise`.

## The test

`driver.f` in this directory is a byte-for-byte copy of the official
`code/fishpack/test/thwscsp.f` (HWSCSP example) with one output section added
right before its `PRINT`/`STOP`: it writes the solution field and the printed
diagnostics as raw little-endian binary (`solution.bin`, `scalars.bin`) instead
of only the few printed digits, and it reads one extra scalar, `DPARAM`, from
`SAB_PARAM_FILE` and adds it to DTHETA. At `DPARAM=0` (`ic/nominal`) the
driver reproduces the official test exactly, including the shipped
`output/darwin.dp` transcript to every printed digit.

The problem is two sequential HWSCSP examples reusing the same theta grid: example 1 is the axisymmetric Poisson solve on a 37x32 (theta,r) grid with a fourth-order colatitude exact solution; example 2 reuses THETA and solves the pole-only Robin/Neumann problem with exact solution R*sin(theta) (test/thwscsp.f). `run.sh` builds `code/fishpack/src` into
`libfishpack.a` with `gfortran -fdefault-real-8 -O2 -std=legacy` (reusing a
build cache shared by every check of the run at the same optimization level;
see `comment/README.md`), links `driver.f` against it, and runs the resulting
executable. The official run time is under 0.1 s; this check declares no
runtime knob because HWSCSP is a non-iterative direct solve on a
compile-time grid (see `run.sh --help`). `run.sh altbuild` builds the same
source at `-O0` instead of `-O2` (same compiler and flags otherwise).

## The two initial conditions

`ic/nominal/param.txt` holds `DPARAM=0.0`, reproducing the official test
unchanged. `ic/variant/param.txt` holds `DPARAM=1.3877787807814457e-17`, exactly two
binary64 ULPs of DTHETA (nominal value 0.043633231299858195), added to
DTHETA before it is used to build the grid and the arrays passed to
HWSCSP. The two files differ byte-wise. On this driver's own scratch
calibration (arm64 macOS, gfortran 15.2), the variant moved the graded fields
by at most 4.441e-16, well inside the finalized bound.

## The pass policy

Pointwise: every value of `solution.bin` (two solution arrays written back to back: example 1's F(1:37,1:32) (1184 f64 values) then example 2's F(1:37,1:33) (1221 f64 values), 2405 f64 values total) and `scalars.bin`
(IERROR1, discretization-error1, IW1 then IERROR2, discretization-error2, IW2 (6 f64 values)) is compared with `|candidate - reference| <= atol + rtol*|reference|`,
`atol=1e-08`, `rtol=1e-08` (`rubric.json`, using the skill's stock
`validate.py` loader, format `f64`). The bound sits several orders of
magnitude above the measured ULP-level floor and several orders below the
routine's own discretization error (7.99842e-04 (example 1), 5.86824e-05 (example 2)), so a wrong coefficient, a
dropped source term or a mis-ordered array in a ported HWSCSP would move
the solution by an amount at least comparable to the discretization error and
fail the bound by a wide margin, while legitimate rounding differences across
compilers and architectures pass.

## Evidence

Scratch calibration on arm64 macOS (gfortran 15.2, `-O2`): variant spread
4.441e-16 (max over every graded value); scalars spread
0.000e+00. The task's own `task selfcheck` on the x86 Debian
trixie worker (`comment/pipeline/self-validation.json`) is the finalized
record; this scratch number is background only, not a calibration on the
target host.

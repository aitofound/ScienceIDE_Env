# tpois3d

Upstream test: `code/fishpack/test/tpois3d.f`. Policy: `pointwise`.

## The test

`driver.f` in this directory is a byte-for-byte copy of the official
`code/fishpack/test/tpois3d.f` (POIS3D example) with one output section added
right before its `PRINT`/`STOP`: it writes the solution field and the printed
diagnostics as raw little-endian binary (`solution.bin`, `scalars.bin`) instead
of only the few printed digits, and it reads one extra scalar, `DPARAM`, from
`SAB_PARAM_FILE` and adds it to DZ. At `DPARAM=0` (`ic/nominal`) the
driver reproduces the official test exactly, including the shipped
`output/darwin.dp` transcript to every printed digit.

The problem is the 3-D Poisson solve POIS3D (FFT in x and y, direct in z) on a 30x30x10 grid, periodic in x and y, exact solution U(x,y,z)=sin(x)*sin(y)*(1+z)**4 (test/tpois3d.f). `run.sh` builds `code/fishpack/src` into
`libfishpack.a` with `gfortran -fdefault-real-8 -O2 -std=legacy` (reusing a
build cache shared by every check of the run at the same optimization level;
see `comment/README.md`), links `driver.f` against it, and runs the resulting
executable. The official run time is under 0.1 s; this check declares no
runtime knob because POIS3D is a non-iterative direct solve on a
compile-time grid (see `run.sh --help`). `run.sh altbuild` builds the same
source at `-O0` instead of `-O2` (same compiler and flags otherwise).

## The two initial conditions

`ic/nominal/param.txt` holds `DPARAM=0.0`, reproducing the official test
unchanged. `ic/variant/param.txt` holds `DPARAM=2.7755575615628914e-17`, exactly two
binary64 ULPs of DZ (nominal value 0.1), added to
DZ before it is used to build the grid and the arrays passed to
POIS3D. The two files differ byte-wise. On this driver's own scratch
calibration (arm64 macOS, gfortran 15.2), the variant moved the graded fields
by at most 1.243e-14, well inside the finalized bound.

## The pass policy

Pointwise: every value of `solution.bin` (the solution array F(1:30,1:30,1:10) (9000 f64 values, I fastest then J then K)) and `scalars.bin`
(IERROR and the discretization error (no printed workspace length for this test)) is compared with `|candidate - reference| <= atol + rtol*|reference|`,
`atol=1e-08`, `rtol=1e-08` (`rubric.json`, using the skill's stock
`validate.py` loader, format `f64`). The bound sits several orders of
magnitude above the measured ULP-level floor and several orders below the
routine's own discretization error (2.93277e-02), so a wrong coefficient, a
dropped source term or a mis-ordered array in a ported POIS3D would move
the solution by an amount at least comparable to the discretization error and
fail the bound by a wide margin, while legitimate rounding differences across
compilers and architectures pass.

## Evidence

Scratch calibration on arm64 macOS (gfortran 15.2, `-O2`): variant spread
1.243e-14 (max over every graded value); scalars spread
3.775e-15. The task's own `task selfcheck` on the x86 Debian
trixie worker (`comment/pipeline/self-validation.json`) is the finalized
record; this scratch number is background only, not a calibration on the
target host.

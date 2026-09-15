# itensor: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

This leaf packages the pinned ITensor v3 C++ tensor-network library as one
cohesive module. It owns the indexed tensor, contraction, MPS/MPO,
quantum-number and iterative-solver implementations.

The check set carries **thirty checks: one per buildable official entry point**.

| family | upstream files | checks |
|---|---|---|
| `unittest/` (the Makefile's default `SOURCES`) | 20 | **20** |
| `sample/` drivers | 9 | **9** |
| `tutorial/` | 10 | **1** |

The twenty unit-test checks each replay the production API calls of exactly one
file under `code/itensor/unittest/` — one check per file, no grouping. The
upstream files assert through Catch2 `CHECK`/`REQUIRE` and expose only a
pass/fail bit, which a check may not grade; `comment/tools/README.md` records how
the probes instead call the same public API on materialised deterministic inputs
and report the numeric observables those assertions bound. The inputs have to be
materialised because `randomITensor()` draws from `std::random_device` and
differs on every process.

The remaining documented exclusions:

- `tutorial/01_one_site`, `02_two_site`, `04_mps` and `05_gates` still carry
  TODO / "Your code here" blocks. Measured confirmation for `05_gates`: the
  binary prints the unchanged Neel-state energy -4.75 because the gate is never
  applied.
- `tutorial/06_DMRG` prints only sweep and bond progress lines, and
  `project_template/myappname.cc` is a build skeleton with no physics.
- `tutorial/finiteT/mettts` and `metts_solution` draw states with
  `Global::random()`; `ancilla` needs the Makefile `app=` variable and an
  external input deck. These stay as documented follow-ups.
- `unittest/bondgate`, `iqtensor` and `webpage` include headers the pinned v3
  tree does not ship (`bondgate.h`, `itensor/iqtensor.h`, `itensor.h`), verified
  with `g++ -fsyntax-only`; `spectrum_test` uses the v2 `Site` tag and a
  `Spectrum` constructor v3 does not provide. The upstream Makefile comments all
  four out for that reason, so `make test-g` never compiles them.
- `unittest/hdf5_test` needs the optional HDF5 build, which the pinned image does
  not install (`h5_open` is undeclared).

Both `sample/hubbard_2d` drivers are packaged, but on a reduced **3x2** lattice
rather than the upstream default 6x3: the default reaches sweep 10/15 with link
dimension 3000 after 88 s of native wall time and was stopped at the 180 s
investigation budget, while the reduced geometry runs in seconds. Each rubric
records the reduction.

Earlier revisions of this leaf carried other exclusions, now superseded:

- ~~23 `unittest/*_test.cc` files cannot become checks~~ — they can, by
  materialising inputs instead of grading the assertion bit.
- ~~`sample/hubbard_2d` and `hubbard_2d_conserve_momentum` exceeded the budget~~
  — the drivers take `[Nx] [Ny] [U]`, so the lattice is a knob.
- `tutorial/01_one_site`, `02_two_site`, `04_mps` and `05_gates` still carry
  TODO / "Your code here" blocks. Measured confirmation for `05_gates`: the
  binary prints the unchanged Neel-state energy -4.75 because the gate is never
  applied.
- `tutorial/06_DMRG` prints only sweep and bond progress lines, and
  `project_template/myappname.cc` is a build skeleton with no physics.
- `tutorial/finiteT/mettts` and `metts_solution` draw states with
  `Global::random()`; `ancilla` needs the Makefile `app=` variable and an
  external input deck. These stay as documented follow-ups.
- `sample/hubbard_2d` and `hubbard_2d_conserve_momentum` exceeded the
  investigation budget: the native run reached sweep 10/15 with link dimension
  3000 after 88 s and was stopped at 180 s.
- `unittest/bondgate`, `iqtensor`, `spectrum` and `webpage` sit under
  commented-out `SOURCES` lines in the pinned `unittest/Makefile`, so the
  official `make test-g` never compiles them.

The `acceleration` label sits on `sample-mixedspin-cc` because that is the
module's most expensive workload (98 s measured under one CPU, against 1-17 s
for the others). Mixed-spin DMRG enables sweep noise and `Global::random()` is
seeded from `time+pid`, so the run is not bit-reproducible by construction; six
independent runs nevertheless reproduced the same converged ground-state energy
to all ten printed decimals, which is what the check grades.

## Build

Each check builds the pinned source and its official driver in a solve-scoped
scratch directory using C++17, g++, and system BLAS/LAPACK. The build may be
reused within one solve through an explicit stamp, but no host build is
trusted. Native macOS evidence is a 132.70 s core build and 35.04 s sample
build; the final Docker record will report build seconds separately from check
run seconds.

## Tolerances

All checks use pointwise comparison of physical numerical outputs. Provisional
bounds are set only after nominal and active-input variant runs in the Docker
calibration; each rubric records the measured spread and the source mechanism
that makes a wrong contraction, sweep or update exceed the bound. No tolerance
is finalized from the two-ULP spread alone, and every later change requires a
fresh self-validation record.

## Blind spots

The leaf does not cover HDF5 serialization, the incomplete tutorial skeletons,
the random METTS drivers, or the default 2D Hubbard workloads that exceeded the
three-minute native investigation budget. These are visible follow-up
candidates rather than silently omitted paths. Cross-platform BLAS and
Linux/x86 source-build behavior remain review items, and no check declares an
alternative build.

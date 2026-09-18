# high-order-assembly: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, the Step 1.2 build-and-run record, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

MFEM's high-order partial assembly and matrix-free operators: for every operator action the
global vector is restricted to element-local vectors, interpolated with its gradients to
quadrature points by one-dimensional tensor contractions, multiplied by the geometric and
coefficient factors stored per quadrature point, contracted back with the transposed basis
and gathered. At order p in d dimensions that is O(p^(d+1)) per element instead of the
O(p^(2d)) of a stored element matrix, and it is the inner loop of every Krylov iteration in a
high-order run. The module owns 45 paths: the partial-, matrix-free- and element-assembly
kernels of the mass, diffusion, vector-diffusion, elasticity and gradient integrators under
`fem/integ/`, the quadrature interpolator (`fem/qinterp/`, `fem/quadinterpolator*.cpp`),
the element and face restriction layer (`fem/restriction.cpp`), the bilinear- and
linear-form extensions that route a form through those kernels (`fem/bilinearform_ext.cpp`,
`fem/linearform_ext.cpp`) and the quadrature-space and quadrature-function types. H(curl)
and H(div) kernels, face/DG kernels, the nonlinear convection kernel, NURBS patch assembly
and the Krylov solvers themselves are owned by other modules of the cut.

The official tests are the documented `Sample runs:` of the examples that drive these
kernels plus the serial Catch2 cases of the module's own unit files, taken under the
approved granularity (one check per upstream target, split only where a target hides
distinct physics): 42 invocations and cases in 21 checks. By target: `ex0` (1 of its 2
runs), `ex1` (16 of 43), `ex2` (5 of 7), `ex7` (2 of 3), `ex29` (2 of 3), `ex39` (2 of 2),
`miniapps/performance/ex1` (4 of 19), and 10 Catch2 cases from `test_pa_kernels.cpp`,
`test_pa_diagonal.cpp`, `test_assembly_levels.cpp`, `test_quadinterpolator.cpp` and
`test_fa_determinism.cpp`. What was left out, and why:

* **Every device-backend run** -- `-d cuda`, `-d raja-*`, `-d occa-*`, `-d ceed-*`, `-d gpu`
  (18 of `ex1`'s runs): the serial build has none of those backends and `-d cuda` aborts in
  `Device::Configure`. This leaf is calibrated on a serial CPU build; the partial-assembly
  path those runs would exercise on a device is instead pinned from below by the four unit
  files (partial against full assembly of the same operator, on the CPU) and from above by
  the templated matrix-free solves of `miniapps/performance/ex1`.
* **Runs that repeat a covered branch on another mesh**: the ten remaining `ex1` runs
  (`star.mesh` default, `periodic-torus-sector.msh`, `star-mixed-p2 -o 2`,
  `fichera-mixed-p2 -o 2`, `disc-nurbs -o -1`, `nc3-nurbs -o -1`, `square-disc-surf`,
  `amr-quad`, `fichera-amr`, `mobius-strip` without `-sc`), the `fichera` and
  `square-disc -o 2` runs of `ex0`, the `beam-quad`, `beam-tet` and `beam-quad-nurbs` runs of
  `ex2`, the `-r 2` runs of `ex29` and the `-src Base` run of `ex39`. Each takes a kernel
  branch (geometry family, curved map, periodic topology, nonconforming mesh, rational
  geometry, static condensation, named attribute sets) that another registered run of the
  same example already takes; the survey kept one run per branch.
* **The two `-amr` runs of `ex7`**: they refine adaptively from an error estimator, which is
  the mechanism owned by `adaptive-mesh-refinement`; the two uniform-refinement legs kept
  here are the ones whose printed L2 error is a fixed-space number.
* **15 of the 19 `miniapps/performance/ex1` runs**: the four kept are the matrix-free
  low-order-refined solve on `fichera` (the module's accelerator path, alone in its check),
  the same problem through the templated full assembly and through the standard assembly
  path (one check, so the two paths are compared under one geometry), and the matrix-free
  solve on the curved `pipe-nurbs`. The other fifteen are the same three assembly modes on
  `star`, `amr-quad`, `amr-hex`, `ball-nurbs` and `disc-nurbs` with and without `-sc`,
  i.e. the same code paths on other meshes; at 50-175 s each they would also put the check
  budget far past what the coverage buys.
* **The parallel examples and every `[Parallel]`, `[CUDA]` and `[MFEMData]` Catch2 case**:
  MPI needs hypre and METIS, which the serial recipe does not build, and the data-tagged
  cases need the external data submodule. Within the five unit files every serial case is in
  a check.

## Build

Yes: every check compiles the pinned MFEM source at solve time. There is nothing prebuilt in
either image.

The recipe is one line, identical for every check --
`make serial -j$SAB_CPUS MFEM_USE_METIS=NO CXXFLAGS="-O3 -std=c++17"` for the library, then
the check's own binary (`ex0`, `ex1`, `ex2`, `ex7`, `ex29`, `ex39`, `miniapps/performance/ex1`
or `tests/unit/unit_tests`) -- where `-O3 -std=c++17` is MFEM's own `OPTIM_FLAGS`
(`config/defaults.mk:29`, taken by `CXXFLAGS ?= $(OPTIM_FLAGS)` at `makefile:227`). The
altbuild appends `-mfma -ffp-contract=fast` and nothing else.

**Reuse within a run.** Each `run.sh` copies `SOURCE_DIR`, takes a SHA-256 over the sorted
relative paths and bytes of that copy plus the exact recipe string, and keys a cache under
`/tmp/sab-build-mfem-high-order-assembly/<hash>`. Within one container `tests/test.sh
produce` runs its checks sequentially, so no cross-check race protocol is needed: on a miss
the arriving check copies the tree into its own cache directory, builds there, and writes
`BUILD_OK` only after `make` succeeds; every later check of the same container finds
`BUILD_OK`, links only the binary it needs against the existing `libmfem.a` (seconds, where
a private build would repeat the library compile) and reports `SAB_BUILD_SECONDS=0`. If the
shared root cannot be created, the same build function runs privately under the check's
`$WORK`, so each `run.sh` stays self-contained. The resource-aware `solve.sh` of skill 5.17
may run several containers at once, sharded by the rubric's `configuration` text; each
container has its own `/tmp`, so each shard pays the library build once and the shards
never share a cache directory (no race, one more compile per shard). The unit checks share
the same key as the example checks, so a container that has built the library for `ex1`
only compiles `unit_tests` on top of it.

**The initial condition is overlaid after the build, not before.** An initial condition here
is a set of mesh files, which no compilation reads. Keying the build on it would give nominal,
variant and altbuild three different keys and force three full library builds where two would
do, so the key covers the pinned source and the recipe only, and the meshes are copied over the
built tree afterwards. The runs read their meshes from that overlaid tree, so nominal and
variant genuinely solve on different geometry while sharing one build.

**Build jobs are the resource knob, fixed at the declared cpus.** Every `run.sh` exposes
`SAB_CPUS` (default 4, the per-check `cpus` of `task.toml`) and passes it to `make -j`; the
examples, the miniapp and `unit_tests` run serially, so the knob bounds only the compile.
The default is never read from the host or the cgroup, as the skill requires (the pyamg leaf,
`tasks/pyamg/classical-amg/comment/README.md`, is the precedent: ninja detected the worker's
88 cores under a `--cpus 1` limit and the OOM killer took out `cc1plus`); `memory_gb` is 4.0
for the four jobs.

Build and run seconds, from the calibration selfcheck of 2026-09-18 on the packaging host
(x86_64, 4 cores; 4 cpus and 4.0 GB per check; record in `comment/pipeline/runtime-metadata.json`):
the resource-aware `solve.sh` packed the 21 checks into one container, and the three solves
took 1808 s, 1742 s and 1718 s wall. The first check of a container pays the library and
`unit_tests` (1378 s at `-j4`); every later check links only its own binary (1-23 s, 89 s in
all), for 1467 s of build per solve against 339 s of graded run time, inside the 900 s budget.
The longest check is `high-order-assembly-path-comparison` at 189 s (two order-3 solves of
the Fichera problem); the two matrix-free checks take 51 s and 62 s and everything else
under 10 s.

## Tolerances

Finalized with the human at STOP 4 on 2026-09-18 from calibration run `20260918T173718Z`
(bounds unchanged from the hypothesis, runtimes re-declared from the measured maxima of the
three solves). Three kinds of stream are graded and they carry different bounds:

| stream | digits | why | bound |
| --- | ---: | --- | --- |
| `sol.gf` of `ex1`, `ex2`, `ex7`, `ex29`, `performance/ex1` | 8 | `sol_ofs.precision(8)` in each source | atol 1e-7, rtol 1e-6 |
| `sol.gf` of `ex0`, `ex39` | 16 | `x.Save("sol.gf")` is `GridFunction::Save(const char*, int precision = 16)`, `fem/gridfunc.hpp:1752` | atol 1e-7, rtol 1e-6 |
| `*__errors.txt` (`ex7`'s `L2 norm of error`, `ex29`'s two L2 errors) | 6 | `cout` with no precision set | rtol 2e-5, a per-file override beside the 8-digit field |
| `unit_results.txt` | integers | verdict and assertion counts | exact |

The numbers are carried over from the nurbs leaf's STOP 4 (same library, same host), where
the eight-digit fields moved by 1e-9 to 1e-6 under a 1e-8 geometry perturbation, every value
an exact multiple of the print quantum. The six-digit override follows the curator's review
of PR #647: a bound of rtol 1e-6 sits below one printed unit for a six-digit value of 1 or
more, which `output-precision-floors-the-bound` forbids. The two sixteen-digit streams keep
rtol 1e-6 so one bound story covers the module; their spread will be an unquantised
round-off number rather than a multiple of 1e-8.

**What the pre-flight measured (native, before any container).** `probe_variant.py` in
the packaging tools ran every registered configuration of the seventeen pointwise checks on
nominal and variant with the packaging host's own build: the fields moved by 2e-10 to 1e-8
(`ex1`), 1e-7 (`ex2`, displacements of order ten on a cantilever), 2e-10 and 2e-9 (`ex39`,
sixteen digits), with the refined mesh's element block identical in every run. The
calibration selfcheck records the same quantity inside the container and is the number the
rubric cites.

**The variant is not the last vertex of every mesh.** MFEM marks triangles and tetrahedra for
refinement by edge length when the mesh is loaded (`mesh/triangle.cpp:73`,
`mesh/tetrahedron.cpp:193`), breaking ties between equal edges by index. On a regular mesh
the last vertex's incident edges are tied with their neighbours, and moving it by 1e-8
flipped a mark: measured on `octahedron.mesh` the refined element block changed and the field
moved by 4.8e-2 by position while its *sorted* values moved by only 3.7e-4 -- a different
discretization, not a perturbed one; `fichera-mixed.mesh` did the same at 1.2e-1. So
`pick_variant.py` chose, per mesh, the first (point, coordinate, sign) whose move leaves the
refined element block unchanged under the native `ex1` and moves the field by at most 1e-6:
the default last point works for 21 of the 23 meshes; `octahedron.mesh` moves its last vertex
along z instead of x, `fichera-mixed.mesh` moves its second-to-last vertex along -x. The
rubrics of the checks that carry those meshes say so. `inline-segment.mesh` lists no vertices,
so its extent `sx` is scaled by 1e-8, which moves the far end by the same amount.

**What the calibration measured.** Reward 1.0. The nominal-versus-variant spread of the
fourteen checks with a live variant is 1e-9 to 1e-7, at most 0.091 of the bound:

| check | spread | bound used | altbuild floor |
| --- | ---: | ---: | ---: |
| elasticity-element-geometries | 1.0e-7 | 0.091 | 1.0e-8 |
| elasticity-order3-static-condensation | 1.0e-7 | 0.090 | 1.0e-7 (0.088) |
| elasticity-nurbs-geometry | 2.0e-8 | 0.082 | 1.0e-10 |
| the three `performance/ex1` checks, poisson element-geometries, nurbs, periodic, static-condensation | 1.0e-8 | 0.050 | 0 to 1.0e-9 |
| poisson-minimal (16 digits) | 4.0e-9 | 0.015 | 2.2e-16 |
| attribute-sets (16 digits) | 1.8e-9 | 0.007 | 1.7e-14 |
| poisson-high-order-curved, poisson-nonconforming-mesh | 1.0e-9 | 0.009 | 0 |
| seven checks with a declared identical variant | 0 | -- | 0, except screened-poisson-sphere 1.0e-14 |

The eight-digit spreads are exact multiples of the print quantum, as on the nurbs leaf; the
two sixteen-digit streams show the unquantised numbers. The container numbers agree with the
native pre-flight to the digit.

**Floors.** The altbuild (`-mfma -ffp-contract=fast` on top of `-O3`) ran on all twenty-one
checks: thirteen were bit-identical to nominal and eight moved, the largest
`elasticity-order3-static-condensation` at 1.0e-7 (0.088 of its bound). The refinement-mark
hazard of the blind spots did not fire: `poisson-element-geometries` was bit-identical under
the altbuild. rtol was not tightened to 1e-7: the elasticity floor would then sit at the bound
and an eight-digit stream would keep only ten printed units of room.

## Blind spots

* **The refined mesh each run writes is not graded** (`refined.mesh`, `displaced.mesh`,
  `sphere_refined.mesh`, `mesh.mesh`). Section 9 of the packaging direction forbids grading
  element numbering after refinement; a genuinely different discrete space is still rejected,
  because the solution stream then has a different length and `validate.py` fails on shape
  before it compares a value.
* **The refinement-mark tie is a sensitivity of the checks themselves, not only of the
  variant.** A candidate whose edge lengths differ from the reference's at the last bit -- an
  FMA in `Mesh::GetLength`, a different summation order -- could flip a longest-edge mark on
  the regular simplex meshes (`octahedron`, `fichera-mixed`, `star-mixed`, `beam-tri`,
  `star-surf`, `escher`) and fail `poisson-element-geometries` or
  `elasticity-element-geometries` by position while computing the right field on a
  differently refined mesh. The altbuild floor is the measurement of that hazard on this
  host: the axis-aligned unit edges and the sqrt(2) diagonals of these meshes are exact in
  double, so contraction should not move them, but the calibration run is what says so.
* **Iteration counts, residual histories and wall clocks never reach `OUT_DIR`.** `ex7` and
  `ex29` print their L2 errors one line away from a CG history, so the printed errors are
  extracted by curated per-binary patterns and never by a generic grep.
* **The five unit checks grade verdicts, not values.** A Catch2 case exposes no numeric
  stream: its XML carries the verdict and assertion counts but also the absolute source path,
  and its `-s` expansion prints values in an unstable format that are often zero-valued
  residuals (partial against full assembly of one operator on one vector). So these checks
  cannot detect a port that stays inside the upstream test's own tolerance while being subtly
  wrong; they are the only checks that exercise the partial-assembly path *as* partial
  assembly on this serial build, which is the accepted cost of having no device.
* **Seven checks have an explicitly identical variant**, so their only calibration evidence
  is the altbuild floor: `screened-poisson-sphere` and `surface-pde` build their geometry in
  code from command-line parameters (perturbing one would be a different configuration), and
  the five Catch2 checks construct their meshes in code. Their rubrics say so rather than
  claiming a spread they do not have.
* **No device backend is exercised.** The module is MFEM's accelerator path, and this leaf
  measures it on the CPU only: the sum-factorized kernels run through the same templates on
  every backend, but the device memory management, the kernel launch layer and the libCEED
  bindings that 18 of `ex1`'s sample runs cover are outside what a serial build can run.
* **Altbuild floors of exactly zero must be read as "not measured".** Where the two builds
  agree bit for bit the code paths contain no multiply-add pair the compiler contracted
  differently; the tolerance then rests on the variant spread and the precision argument
  alone. A second compiler in the image is the stronger alternative the pitfalls index
  recommends; considered and deferred, as on the nurbs leaf.

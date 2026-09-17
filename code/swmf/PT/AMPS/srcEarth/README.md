# SEP-in-geospace model: compact global fields in Mode3D


## Selecting the background-field epoch

The global background-field snapshot/reference time is selected in the input file with
`EPOCH` inside `#BACKGROUND_FIELD`:

```text
#BACKGROUND_FIELD
FIELD_MODEL  T96
EPOCH        2010-01-01T00:00:00
```

The recommended format is `YYYY-MM-DDTHH:MM:SS`.  This epoch is used when initializing
Geopack/IGRF coefficients, Tsyganenko dipole tilt, SPICE coordinate rotations, and the
Mode3D field snapshot.  It can be overridden without editing the input file:

```bash
./amps -mode gridless -i AMPS_PARAM.in --epoch 1965-01-01T00:00:00
./amps -mode 3d       -i AMPS_PARAM.in --epoch 2010-01-01T00:00:00
```

The precedence is `--epoch` > input-file `EPOCH` > the compiled default
`2000-01-01T00:00`.  A timestamp containing a space must be quoted in the shell.

## Purpose

Mode3D backward calculations trace an energetic particle through the complete AMR domain. The AMPS mesh uses normal MPI domain decomposition: every rank has the global AMR tree, but only the owner domain and a limited neighbor layer have allocated `cDataBlockAMR` objects. A trajectory assigned to one MPI rank can enter any leaf of the global tree, so field interpolation must not depend on whether that leaf's block is allocated locally.

The previous implementation solved this by allocating every used AMR block on every MPI rank, gathering the magnetic field, and populating all replicated interior and ghost center nodes. That approach was correct for field access but replicated complete AMPS cell-associated state vectors and ghost storage. Its memory cost was much larger than the physical B field required by cutoff-rigidity and density/flux backtracing.

Mode3D now keeps the standard distributed AMPS mesh and replicates only compact cell-centered magnetic and electric field arrays. AMPS linear interpolation is evaluated with the decomposition-independent row stencil introduced in `src/pic`.

## Modified files

- `3d/GlobalMagneticField.h`
- `3d/GlobalMagneticField.cpp`
- `3d/CutoffRigidityMode3D.cpp`
- `3d/CutoffRigidityMode3D.h`
- `3d/Mode3D.cpp`
- `3d/DensityMode3D.cpp`
- `3d/DensityMode3D.h`
- `3d/Mode3DParallel.cpp`
- `3d/Mode3DParallel.h`
- `3d_forward_swmf/Mode3DForwardSWMF.cpp`
- `3d_forward_swmf/Mode3DForwardSWMF.h`

## Global cell indexing

AMPS replicates the AMR tree topology on every MPI rank. The temporary tree-node member `Temp_ID` is therefore used as a dense global leaf index.

Before each field snapshot is assembled, `GlobalMagneticField.cpp` recursively resets `Temp_ID` to `-1` on every tree node. It then traverses the tree in deterministic child order and assigns consecutive IDs to leaves satisfying `IsUsedInCalculationFlag`.

For a block containing `Nx`, `Ny`, and `Nz` interior cells, the scalar global cell index is

```text
cellsPerBlock = Nx * Ny * Nz
localCell     = i + Nx * (j + Ny*k)
globalCell    = node->Temp_ID * cellsPerBlock + localCell
```

The vector components are stored at `3*globalCell + component`.

`Temp_ID` is shared AMPS scratch storage. No other algorithm may overwrite it between compact-field assembly and completion of the corresponding backward calculation. Field evaluation checks every row-stencil node ID and terminates with a diagnostic if an invalid or stale `Temp_ID` is detected.

## Compact arrays

The implementation owns three process-local arrays that are identical on all MPI ranks after assembly:

```cpp
std::vector<double> GlobalMagneticField_; // 3 * NglobalCells
std::vector<double> GlobalElectricField_; // 3 * NglobalCells
std::vector<int>    GlobalCellPresence_;  // 1 * NglobalCells
```

Only physical interior cells are stored. There are no global ghost cells and no replicated AMPS data blocks.

The persistent field memory per MPI rank is approximately

```text
B: 24 * NglobalCells bytes
E: 24 * NglobalCells bytes
presence: 4 * NglobalCells bytes
```

The presence array is retained for defensive validation. Every used physical cell must be contributed by exactly one owner rank.

## Assembly procedure

The main entry point is

```cpp
Earth::Mode3D::GlobalMagneticField::AssembleCellCenteredFieldsForCutoff(
    diagnosticTag,
    magneticFieldDataOffset,
    electricFieldDataOffset,
    plasmaVelocityDataOffset,
    verbose);
```

The procedure is:

1. Mark the previous snapshot unavailable.
2. Reset `Temp_ID` throughout the global AMR tree.
3. Assign deterministic dense IDs to all used leaves.
4. Allocate compact B, E, and presence arrays.
5. Visit only leaves for which `node->Thread == PIC::ThisThread`.
6. Pack each allocated owner interior cell into its `Temp_ID`-based global location.
7. Use chunked in-place `MPI_Allreduce` operations to replicate B, E, and the presence counts.
8. Verify that every cell has exactly one owner contribution.
9. Mark the snapshot ready and keep the arrays read-only during trajectory tracing.

No call to `AllocateBlock()` is made by the compact-field module.

### Standalone Mode3D

`Mode3D::InitMeshFields()` evaluates the configured standalone B and E models on owner-rank AMPS blocks.

#### Background-field initialization progress

Both serial and POSIX-thread standalone field initialization report one **global**
completion bar on MPI rank 0.  The display deliberately matches the cutoff-rigidity
progress bar:

```text
[Mode3D field INITIALIZATION] [rank 0/global over 6 MPI ranks] [##########--------------------------] 27.8%  (Cell 981/3530)  ETA 00:06:14
```

The bar width is 36 characters, with `#` for completed work and `-` for work still
remaining.  Field initialization prints normal updates every **2 seconds**, i.e. at
half the frequency of the one-second cutoff-progress display; startup and completion
are forced so the user always sees 0% and 100%.  Each printed line is explicitly
flushed.  This matters for C19 production runs launched through `mpirun` or batch
schedulers, where newline buffering can otherwise delay progress output.

The progress count represents owner-cell slots completed over **all MPI ranks**, not
work merely assigned.  In pthread mode each temporary worker increments a rank-local
`std::atomic<long long>` only.  The original MPI-rank thread periodically publishes
the aggregate local delta through `DynamicMpiProgressCounter` and is the only thread
that calls MPI.  This design gives live threaded progress without requiring
`MPI_THREAD_MULTIPLE`.  After its own share is finished, the rank/main thread continues
polling until its temporary workers and all remote ranks have published their final
counts, then emits the exact 100% line.

`Mode3DPrepareMagneticFieldSnapshot()` then calls

```cpp
const long int eOffset =
    PIC::CPLR::DATAFILE::Offset::ElectricField.active ?
    Earth::Mode3D::GlobalMagneticField::DataFileElectricFieldDataOffset() : -1;

Earth::Mode3D::GlobalMagneticField::AssembleCellCenteredFieldsForCutoff(
    "Mode3D",
    Earth::Mode3D::GlobalMagneticField::DataFileMagneticFieldDataOffset(),
    eOffset,
    -1,
    true);
```

When the DATAFILE electric-field slot is inactive, the compact E array is explicitly zero.

### SWMF-coupled Mode3D

SWMF stores magnetic field and plasma velocity in separate coupler-buffer locations. It does not store an independent electric-field vector. `PrepareGlobalSWMFCoupledMagneticFieldForCutoff()` therefore calls

```cpp
Earth::Mode3D::GlobalMagneticField::AssembleCellCenteredFieldsForCutoff(
    "Mode3DForwardSWMF",
    PIC::CPLR::SWMF::MagneticFieldOffset,
    -1,
    PIC::CPLR::SWMF::BulkVelocityOffset,
    true);
```

For each owner cell, the compact electric field is calculated as

```text
E = -v x B
```

Both offsets and completion of the first SWMF coupling receive are checked before assembly.

## Row-stencil interpolation

The public field evaluators are

```cpp
bool InterpolateMagneticField(const double* x,cAMRNode* node,double* B);
bool InterpolateElectricField(const double* x,cAMRNode* node,double* E);
```

They build a stack-local row stencil:

```cpp
PIC::InterpolationRoutines::cRowStencil row;
PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,row);
```

Each row element contains

```cpp
row.Element[s].node;
row.Element[s].i;
row.Element[s].j;
row.Element[s].k;
row.Element[s].Weight;
```

The owning block can be unallocated on the current MPI rank. The field value is obtained directly from the compact array:

```cpp
for (int s=0;s<row.Length;s++) {
  const auto& e=row.Element[s];

  const long int localCell =
      e.i + _BLOCK_CELLS_X_*(e.j + _BLOCK_CELLS_Y_*e.k);

  const long int globalCell =
      e.node->Temp_ID*cellsPerBlock + localCell;

  B[0] += e.Weight*GlobalB[3*globalCell+0];
  B[1] += e.Weight*GlobalB[3*globalCell+1];
  B[2] += e.Weight*GlobalB[3*globalCell+2];
}
```

The row stencil preserves the AMPS cell-centered linear interpolation geometry, including:

- ordinary eight-cell trilinear interpolation;
- same-level block-boundary interpolation;
- coarse/fine multiblock reconstruction;
- averaging of the corresponding `2 x 2 x 2` fine cells for a logical coarse support point;
- continuous coarse/fine transition blending.

Unlike the legacy pointer stencil, a remote cell is not omitted merely because its center-node object is unavailable locally.

## Updated Mode3D field evaluator

`cMode3DMeshFieldEval::GetB_T()` now:

1. finds the containing leaf in the global AMR tree using its private `lastNode_` hint;
2. accepts a valid leaf even when `node->block == NULL`;
3. calls `GlobalMagneticField::InterpolateMagneticField()`;
4. returns the interpolated global-array value.

The evaluator no longer branches between DATAFILE and SWMF pointer-stencil access. The source-specific work is performed once during snapshot assembly. During particle tracing the field path is identical for standalone and coupled calculations.

The optional direct analytic-field debug path remains available and bypasses the compact arrays.

## Mode3D cutoff task-level MPI/thread parallelization

### Why the scheduler was changed

The Mode3D cutoff solver previously used one **observation location** as the smallest
schedulable unit.  That works well for global shell scans containing many locations, but
it performs poorly for directional products with only one or a few locations.  C19 is
the clearest example: one GOES observation location with a 10-degree directional map has
684 independent directional trajectories plus one primary scalar cutoff.  The old
scheduler reported 16 configured workers but reduced the local worker count to one
because its chunk contained only one location.

The cutoff solver now schedules **independent trajectory tasks**.  For each location the
flat task layout is:

```text
ordinary cutoff, no directional map:
    task 0 = primary scalar cutoff

ordinary cutoff with DIRECTIONAL_MAP:
    task 0 = primary scalar cutoff
    task 1 = directional cell 0
    task 2 = directional cell 1
    ...

P1 direct directional access companion
(PENUMBRA_SCAN + DIRECTIONAL_MAP + CUTOFF_RIGIDITY_LIST_GV):
    task 0                         = primary scalar cutoff
    task 1 .. Ndir                 = directional PENUMBRA_SCAN cells
    remaining Ndir x Nrig tasks   = exact three-state A(R,Omega) samples

RIGIDITY_LIST:
    task 0 = requested rigidity 0
    task 1 = requested rigidity 1
    ...
```

For a normal directional run,

```text
tasks_per_location = 1 + number_of_directional_cells
total_tasks         = number_of_locations * tasks_per_location
```

When the P1 direct-access companion is requested, the exact fixed-rigidity
classifications are independent trajectory tasks as well:

```text
tasks_per_location = 1 + Ndir + Ndir*Nrig
```

This intentionally increases the trajectory count: C19B needs the energy/direction
access function itself rather than inferring detector transmission from one scalar
cutoff per sky cell.

and the global task identifier is decoded without allocating a task list:

```text
location_id = task_id / tasks_per_location
local_task  = task_id % tasks_per_location
```

This same task space is used by `DYNAMIC`, `BLOCK_CYCLIC`, and `STATIC` MPI
schedulers.  Consequently a one-location calculation can use multiple MPI ranks.
Inside each rank, `THREADS` or `OPENMP` distributes the rank's task range among the
configured shared-memory workers.

### Why concurrent trajectories are safe

The background field is **not** a single mutable `B` value shared by all workers.  Before
backtracing, Mode3D assembles the complete spatial field snapshot into compact global
arrays representing `B(x,y,z)` (and `E(x,y,z)` when present).  Those arrays are identical
on all MPI ranks and remain read-only during trajectory integration.

Every worker owns a private `cMode3DMeshFieldEval` object.  In particular, the worker's
`lastNode_` search hint, interpolation row stencil, and local diagnostic accumulation are
not shared.  A worker tracing trajectory A can therefore interpolate `B(x_A)` while a
second worker simultaneously interpolates `B(x_B)` from the same immutable field
snapshot.  Neither operation changes the field seen by the other trajectory.

The output arrays are also lock-free by construction because each flattened task owns a
unique result element:

```text
primary task                 -> rcRank[location], eminRank[location], band diagnostics
directional task             -> dirMapRank[location, cell]
P1 direct directional task   -> dirAccessStateRank[location, cell, rigidity]
RIGIDITY_LIST task           -> accessStateRank[location, rigidity]
```

No two independent tasks write the same slot.  Final rank-local arrays continue to be
combined with the existing sentinel-aware MPI reductions.

### MPI thread level

Worker threads never call MPI.  Only the MPI rank/main thread fetches dynamic work,
updates the completion counter, and performs reductions.  The implementation therefore
does not require `MPI_THREAD_MULTIPLE`; it remains compatible with the existing
FUNNELED/single-thread MPI use pattern.

### Dynamic chunk meaning for cutoff

`MODE3D_MPI_DYNAMIC_CHUNK` / `-mode3d-mpi-dynamic-chunk` is now interpreted by the
Mode3D **cutoff** solver as the number of flattened cutoff tasks fetched per atomic MPI
request.  Density/flux continues to interpret the same generic control in its own work
units, which are observation locations.

For the direct `THREADS` backend, use a cutoff chunk at least as large as the requested
thread count.  Otherwise one fetched chunk cannot feed all local workers.  The solver
prints a warning when the dynamic cutoff chunk is smaller than the thread count.  For
example, the current C19 runner commonly requests 32 tasks with `-nt 16`.

With `0` (automatic), the generic resolver selects approximately four work items per
configured worker, capped by the total job size.

### C19 example

At the current C19 directional resolution the underlying regular sky grid is:

```text
longitude cells = 360 / 2.5 = 144
latitude cells  = 180 / 2.5 + 1 = 73
full-sphere directional cells = 144 * 73 = 10,512
```

C19 does **not** trace all 10,512 cells by default. It uses the generic
`DIRMAP_COVERAGE VECTOR_APERTURES` selector. One or more instrument LOOK boresights,
roll/up vectors, and elliptical half-angles are supplied in SM, GSM, or the local SM
basis. The heads can point anywhere and are not assumed to be east/west or antipodal.
The selector retains a subset of the same regular SM lon/lat cells, so retained
trajectories are exactly the ones a `FULL_SPHERE` run would have calculated.

For independent GRIDLESS cases the C19 runner creates an epoch-specific
`C19_directional_apertures.dat`. The default GRIDDED batch creates one combined file
whose records are qualified with `LOCATION=<global-trajectory-row>`. Mode3D resolves
those records into a **separate directional-cell list for each active location**. A
compact union is retained only for rectangular storage/MPI reductions; the scheduler
traces, and the Tecplot writer emits, only the cells belonging to the current location.
Consequently a GOES-13 map in a simultaneous GOES-13/GOES-15 snapshot contains only the
GOES-13 detector apertures rather than the four-lobe union of both spacecraft.

When the observational reference records telemetry-head provenance, the runner maps
each numerator/denominator stream to that actual head ID and uses its epoch-specific
attitude vector; no direction is inferred from a head name. For the current P4/P5 case,
the pruning envelope uses the widest P5 half-angles (30° horizontal, 60° vertical) for
each active detector head. Two roughly opposite GEO heads typically retain about
1,700--1,900 directions (~17% of the full sphere) **per spacecraft/location**, but the
count follows the actual attitude vectors.

The current uncorrected-flux response is not limited to the nominal P4/P5 channel
boundaries.  It includes the documented high-energy secondary proton response through
150 MeV for P4 and 190 MeV for P5.  `--access-energy-points 48` therefore builds a
logarithmic 15--190 MeV science grid and inserts the *exact* response boundaries; with
the committed response this gives 55 actual energy nodes (about 0.168--0.627 GV for
protons).  The former infinitesimal `E*(1+/-1e-8)` response-edge trajectories are no
longer needed because the runner integrates the piecewise detector response exactly.

For about 1,750 selected directions the scheduled work is approximately:

```text
primary scalar cutoff             =       1
directional PENUMBRA cells        =   1,750
direct A(R,Omega) samples         = 1,750 * 55 = 96,250
-----------------------------------------------------
total tasks/location              ~ 98,001
```

The historical full-sphere setting would schedule:

```text
1 + 10,512 + 10,512 * 55 = 588,673 tasks/location
```

so vector-aperture pruning still reduces scheduled directional work by roughly a factor
of six for this geometry while preserving angular resolution inside the viewed
apertures. In a multi-spacecraft GRIDDED snapshot, these task counts are now computed
per location rather than multiplying every location by the union of all spacecraft
apertures. If two spacecraft each require about 1,750 cells but their apertures are
disjoint, the expensive trajectory count is therefore approximately `2 x 1,750`, not
`2 x 3,500`.

The direct-access states remain discrete.  C19 does **not** linearly interpolate an
ALLOWED/FORBIDDEN state change between two sampled rigidities.  Instead, the complete
response-weighted interval is carried as an explicit lower/upper uncertainty bracket.
`--max-discrete-transition-fraction` (default 0.05) prevents a quantitative E/W result
from being accepted when too much detector response falls inside such transition
brackets.  Increasing `--access-energy-points` is therefore an explicit rigidity-grid
convergence test rather than a hidden change in the interpolation model.

Both AMPS interfaces are supported:

```text
# input file
DIRMAP_COVERAGE VECTOR_APERTURES
DIRMAP_APERTURE_FILE instrument_apertures.dat
```

with file rows

```text
name frame bx by bz upx upy upz horizontalHalfDeg verticalHalfDeg [LOCATION=<index>]
```

or repeatable inline definitions such as

```text
DIRMAP_APERTURE HEAD_A SM 0.10 0.97 0.22 0.00 0.22 0.98 30 60
```

The equivalent command line is:

```bash
-cutoff-dirmap-coverage VECTOR_APERTURES \
-cutoff-dirmap-aperture-file instrument_apertures.dat
```

with repeatable `-cutoff-dirmap-aperture "..."` available for inline definitions.
Set `DIRMAP_COVERAGE FULL_SPHERE` (or the corresponding CLI value) to recover the
complete sky.

The optional location qualifier is backward compatible: an unqualified aperture
applies to every output location. In a Mode3D `SNAPSHOT_LIST` batch, qualified rows
belonging to inactive epochs are removed and active global row numbers are remapped to
the snapshot-local location IDs used by the output files.

A run with four MPI ranks and 16 threads per rank can expose up to 64 concurrent
trajectory workers even for one GOES location. Actual speedup depends on trajectory-
length imbalance, CPU affinity, memory/cache behavior, and the selected particle mover.


### C19 current single-workflow implementation

C19 no longer exposes P0/P1/P2 as alternate runner modes. Those labels are retained only
as development history. `srcEarth/test/C19/run_C19.py` now has one science-processing
path with two explicit trajectory products.  The production default is the optimized
`DIRECT_ACCESS` product; `PENUMBRA_SCAN` remains the full cutoff-topology diagnostic.
The validated production behavior includes:

- `DIRECT_ACCESS` with trace-limit outcomes preserved as `UNRESOLVED`;
- adaptive per-direction rigidity refinement by default, with the dense fixed grid
  retained through `--no-adaptive-access` as a convergence/reference calculation;
- direct three-state directional `A(R,Omega)` output for both GRIDDED Mode3D and GRIDLESS;
- detector-response folding with event-derived spectrum/provenance support;
- default `2.5° × 2.5°` directional sampling;
- default vector-aperture directional work (`VECTOR_APERTURES`), using the actual
  per-epoch detector look vectors and a conservative P5-sized pruning envelope, with
  `FULL_SPHERE` retained as an explicit diagnostic alternative;
- default Mode3D mesh resolution `0.025 Re` near Earth and `1.0 Re` at the boundary;
- default GRIDDED `SNAPSHOT_LIST` batching: one Mode3D process and mesh allocation per
  field model/search configuration, with B/E rebuilt only for each unique epoch;
- explicit detector attitude (`SM_PROXY` or per-epoch FILE) and optional bounded upstream
  anisotropy in the synthetic-observation fold;
- staged execution/trajectory/fold/observational validity; and
- standard comparison/diagnostic plot generation after normal post-processing, with
  each plot family failure-isolated so one Matplotlib exception cannot suppress later
  transmission, cutoff, spectrum, or aperture diagnostics;
- guaranteed recovery of missing `C19_comparison_*` and `C19_scatter_*` core figures
  from already constructed `ModelRow` data. The recovery renderer runs after the richer
  primary comparison family, never overwrites an existing primary figure, uses the same
  calculated/accepted/bounds/cutoff selector, and therefore also works with `--skip-run`;
- homogeneous Matplotlib time-axis units: selected reference UTCs and model UTCs are
  both parsed to `datetime`, avoiding the ISO-string/datetime unit-conversion failure
  that could previously abort the comparison family before scatter generation.

There is no `--p0-diagnostic` or `--p2-diagnostic` flag. GRIDLESS/GRIDDED selection,
SMOKE/ROUTINE/FULL cadence, detector attitude, anisotropy, and numerical resolutions are
inputs to the same current workflow rather than separate implementations.
`--cutoff-search` intentionally selects only the two current products (`DIRECT_ACCESS`
or `PENUMBRA_SCAN`); historical trace-limit/fold behavior is not re-exposed.  The runner
fixes those internal semantics to the current validated contract and records them in the
generated inputs/results.

The C19 `ROUTINE` profile intentionally samples the five-minute observational reference
at 60-minute spacing.  Use `--profile FULL` or `--time-step-minutes 0` when a modeled
point is required for every valid selected reference epoch; GRIDDED snapshot batching
still reuses one Mode3D mesh per field model.  `C19_model_coverage.csv` records whether
each requested reference row produced an accepted direct scalar, direct bounds only, a
cutoff-midpoint diagnostic, or no model row.

For DIRECT_ACCESS the cutoff comparison now distinguishes `Rc_effective` from
`Rc_midpoint_diagnostic`.  The former is withheld when unresolved trajectory samples are
present.  The latter is an explicitly labelled blocked-area midpoint used only in plots
and the diagnostic hard-cutoff fold; rigorous `Rc_lower/Rc_upper` bounds remain visible
and the midpoint never enters direct acceptance metrics.

DIRECT_ACCESS comparison reporting also distinguishes the **calculated direct scalar**
from the **accepted direct scalar**.  `direct_calculated_*` is retained whenever the
detector fold produces a finite central E/W value, even if a later scientific gate
rejects it.  The historical `modeled_*` fields remain acceptance-only so existing metrics
do not change.  When no scalar can be formed but finite rigorous ratio bounds exist,
`direct_bound_midpoint_*` supplies an explicitly diagnostic plotting location.  Scatter,
parity, residual, and time-series plots consume one shared `direct_plot_groups()`
classifier, which prevents status-filter drift between plot families.  The corresponding
serialized coverage levels are `DIRECT_ACCEPTED`, `DIRECT_CALCULATED_NOT_ACCEPTED`, and
`DIRECT_BOUNDS_ONLY`; `C19_result.json:plot_consistency` audits the population counts.
A normal run records convergence as `NOT_TESTED` rather than treating an unexecuted
convergence campaign as a failed direct result.

The current GRIDDED command includes the fine mesh defaults, while both GRIDDED and
GRIDLESS generated inputs contain the identical adaptive seed rigidity list used to
produce `A(E,Omega)`.  Both solvers call the shared `util/AdaptiveDirectAccess.h`
algorithm: all seeds are evaluated, guard midpoints probe hidden structure, and only
visible state-changing intervals are recursively refined to the configured maximum
depth.  Realized internal nodes may therefore differ by direction, but the algorithm,
seed/support contract, and post-processing are identical.

GRIDDED batching uses the existing Mode3D multi-snapshot lifecycle but adds explicit
irregular epochs and epoch-scoped locations. `amps_init_mesh()`, `amps_init()`, and
static sphere setup remain outside the snapshot loop. For each sorted unique epoch,
Mode3D interpolates the driver, filters the multi-row trajectory to matching samples,
remaps location-qualified apertures, refills owner-block B/E, assembles the compact
global arrays, and runs the cutoff product. Compact B/E/presence vector storage is
resized only if the invariant mesh dimensions change and is otherwise cleared/reused
in place; shared scratch `Temp_ID` values are deliberately reassigned each snapshot.
This filtering prevents the ordinary
TIME_SERIES `N_snapshot x N_location` Cartesian product. `--gridded-batch OFF` retains
the historical one-process-per-case layout for regression comparison, and other tests
are unaffected unless they explicitly request `TEMPORAL_MODE SNAPSHOT_LIST`.

`SNAPSHOT_LIST` also supports `OUTPUT_MODE SHELLS`. A shell domain has no timestamped
locations to filter: every configured shell is evaluated at every listed epoch, while
the AMR topology is allocated once and the B/E state is rebuilt inside the snapshot
loop. Each shell product receives the same deterministic snapshot-index/UTC suffix used
by other multi-snapshot Mode3D outputs, preventing cross-epoch overwrites.

Standalone GRIDLESS is mesh-free: the early `-mode gridless` dispatch
returns before `amps_init_mesh()` and evaluates the background field directly along
trajectories.  GRIDLESS cutoff/direct-access tasks now use the same MPI + intra-rank
THREADS/OPENMP/SERIAL backend controls as Mode3D (`GRIDLESS_PARALLEL`,
`GRIDLESS_THREADS`); only the rank/main thread calls MPI.  Multi-epoch TRAJECTORY inputs
conservatively fall back to serial intra-rank direct-field evaluation because Geopack
snapshot state is process-global. GRIDLESS writes `cutoff_gridless_dir_access_point_####.dat`; batched Mode3D
writes `cutoff_3d_dir_access_loc_######<snapshot-suffix>.dat`, and C19 resolves the
suffix/local-ID mapping through `C19_batch_manifest.csv` before folding either schema through the
same postprocessor. Standard outputs
include `C19_comparison_*`, `C19_scatter_*`, `C19_parity_*`, `C19_residual_*`,
`C19_transmission_*`, and `C19_aperture_diagnostic.png` for every completed normal run.

## Error handling

The compact-field implementation treats the following conditions as fatal consistency errors:

- an owner-cell gather misses a used interior cell;
- more than one rank contributes the same used interior cell;
- a row-stencil entry contains a non-interior index;
- a row-stencil node has an invalid `Temp_ID`;
- a requested global cell is absent from the assembled snapshot.

A missing row point is not removed and the remaining interpolation weights are not renormalized. Doing so would change the field and could make cutoff results depend on MPI decomposition. Row-point removal remains available to external applications through `cRowStencil`, but it must be an explicit physical masking decision rather than an automatic response to missing distributed data.

A field interpolation call returns `false` only when:

- no compact snapshot has been assembled;
- the requested point is outside the used AMR tree; or
- AMPS cannot construct an interpolation row.

## Compatibility

The old function

```cpp
MaterializeCellCenteredMagneticFieldForCutoff(tag,bOffset,verbose)
```

is retained as a B-only wrapper. It now assembles compact B plus a zero E array.

The old debug signature

```cpp
RedefineAllAllocatedMagneticField(...)
```

is also retained so existing source code compiles. Its implementation delegates to `RedefineGlobalMagneticField()` and intentionally does not allocate remote blocks.

Existing forward Monte Carlo transport continues to use the normal distributed AMPS cell buffers and is not redirected through these compact arrays. The change is specific to Mode3D backward products that require arbitrary global field access.

## Recommended validation

The following comparisons should be run before removing any legacy reference branch from a development tree:

1. **Uniform mesh:** compare old pointer-stencil and new row-stencil B at random positions.
2. **Same-level block faces:** sample both sides and exactly on the transition region.
3. **AMR coarse/fine faces, edges, and corners:** compare every physical row entry and final B.
4. **MPI decomposition invariance:** run the same snapshot with 1, 2, 4, and 8 ranks and compare fields and cutoff products bitwise where reduction order permits, otherwise to roundoff.
5. **Standalone DIPOLE:** compare interpolated B and cutoff maps with the former replicated-block implementation.
6. **SWMF snapshot:** compare compact B and derived E against direct owner-cell values.
7. **Time series:** verify that `Temp_ID` is reset and arrays are rebuilt for every field snapshot.
8. **Snapshot-list batching:** prove mesh initialization occurs once, each trajectory
   row matches exactly one snapshot, shared-epoch spacecraft use one field fill, and
   batched products equal independent-case products.
9. **Memory scaling:** confirm that increasing MPI rank count does not allocate additional global AMPS blocks and that per-rank growth is limited to compact B/E/presence arrays.

## Mode3D DIPOLE magnetic-field interpolation error statistics

For a mesh-backed `FIELD_MODEL DIPOLE` calculation, the exact analytic field is known at
all trajectory sample coordinates. Mode3D now uses this reference to measure the error of
the actual compact-array and row-stencil field value returned to the particle mover.

For each successful mesh-field evaluation, the diagnostic records

```text
relative_error = |B_mesh - B_dipole| / |B_dipole|
```

The statistics are sample-weighted. A coordinate visited repeatedly by the trajectory
integrator contributes repeatedly because those are the field determinations that affect
the calculated trajectory. Every field evaluator keeps a private count, error sum,
maximum error, and maximum-error coordinate. It merges those values into a rank-local
accumulator only when the evaluator is destroyed, avoiding a mutex in the high-frequency
field-evaluation path. At the end of cutoff or density/flux processing, MPI reductions
produce the global sample count, mean relative error, and maximum relative error.
`MPI_MAXLOC` identifies the rank containing the maximum and that rank broadcasts the
associated location.

Rank zero prints a summary similar to

```text
========== Mode3D DIPOLE magnetic-field interpolation error ==========
Calculation                 : Mode3D cutoff rigidity
Definition                  : |B_mesh-B_dipole|/|B_dipole|
Number of field samples     : ...
Sample mean relative error  : ...
Maximum relative error      : ...
Max-error location [m]      : x y z
Max-error location [km]     : x y z
Max-error location [Re]     : x y z
=======================================================================
```

The diagnostic is automatically enabled only when

- the selected field model is `DIPOLE`; and
- Mode3D uses mesh-backed field evaluation.

It is disabled for `-mode3d-field-eval ANALYTIC`, because that path directly returns the
reference field and would only report a trivial zero interpolation error. A new statistics
window is started independently for each cutoff and density/flux calculation, including
each separately processed time snapshot.

## F3 structured tracing and backward-compatible cutoff behavior

The F3 density/flux implementation required more information than the historical
Boolean cutoff classifier could provide.  A trajectory can now terminate as
allowed, physically forbidden, capped by a numerical safety limit, or failed
numerically.  At the same time, pre-existing cutoff products and C-series
reference tests depend on the historical convention that a trajectory which does
not escape before a configured time/step/distance cap is Boolean forbidden.

The implementation preserves both requirements through caller-specific policies
rather than globally redefining a termination state.

### Structured versus Boolean interfaces

Structured density/diagnostic callers use:

```cpp
Earth::GridlessMode::TraceTrajectoryShared(...)
Earth::GridlessMode::TraceTrajectorySharedEx(...)
Earth::Mode3D::TraceTrajectoryMesh(...)
```

They receive `TrajectoryResult` and preserve `TIME_LIMIT`, `STEP_LIMIT`, and
`DISTANCE_LIMIT` as unresolved.

Cutoff searches and older callers use the Boolean interfaces:

```cpp
Earth::GridlessMode::TraceAllowedShared(...)
Earth::GridlessMode::TraceAllowedSharedEx(...)
Earth::Mode3D::TraceAllowedMesh(...)
Earth::Mode3D::TraceAllowedMeshEx(...)
```

plus private Mode3D/gridless cutoff wrappers.  Those interfaces map inner impact,
validated trapping, and configured trace limits to `false`; outer escape maps to
`true`.  Invalid steps, invalid fields, and numerical failures are retried once
with stricter settings and throw if they remain failures.

The shared helpers in `util/TrajectoryTermination.h` make this distinction
explicit.  `IsResolvedTermination()` remains unchanged for F3.  The new
`IsCutoffForbiddenTermination()` is applied only at Boolean cutoff boundaries.

### Internal integration policies

Both Mode3D and gridless tracing select one of two policies:

```text
StructuredAccurate       F3 and structured density/diagnostic APIs
LegacyCutoffCompatible   Boolean cutoff APIs and C-series regressions
```

`StructuredAccurate` treats all timestep restrictions as upper bounds, removes
the old `100 km/v` minimum-step floor and asymptotic boundary-distance limiter,
and classifies the first analytic intersection of each accepted trajectory chord
with the inner sphere or outer box.  This provides the unambiguous termination
accounting required by F3.

`LegacyCutoffCompatible` retains the pre-F3 boundary-distance limiter,
`100 km/v` floor, and cutoff endpoint behavior.  It is intentionally confined to
Boolean cutoff calculations so existing dipole cutoff/penumbra references are
not changed by the F3 accuracy correction.

### Safety limits and retry behavior

For structured results:

```text
TIME_LIMIT, STEP_LIMIT, DISTANCE_LIMIT -> unresolved and reported
```

For Boolean cutoff results:

```text
TIME_LIMIT, STEP_LIMIT, DISTANCE_LIMIT -> false
```

Limit outcomes are not retried by cutoff wrappers.  Genuine numerical outcomes
(`INVALID_TIME_STEP`, `INVALID_FIELD`, `NUMERICAL_FAILURE`) receive one retry
with half `DT_TRACE`, up to twice the step count, and twice the effective time
cap.  This prevents a normal trapped low-rigidity trajectory from aborting an
entire shell while retaining fail-fast diagnostics for an actual field or mover
problem.

### Descending upper-cutoff scan

The penumbra-safe `UPPER_SCAN` now evaluates its unchanged logarithmic rigidity
grid from `Rmax` downward.  It stops at the first forbidden sample and bisects the
bracket between that sample and the allowed sample immediately above it.  This
returns the same highest forbidden-to-allowed transition as a complete scan, but
skips lower-rigidity samples that cannot change `Rc_upper` and that are commonly
the most expensive MESH trajectories.

This change is important for C2/C3/C11 performance: location-level progress no
longer waits for every low-rigidity trapped trajectory before the upper branch is
identified.

### Validation documentation

Detailed behavior, parameter semantics, and the C1/C2/C3/C11/F3 validation
matrix are documented in:

```text
srcEarth/test/README.md
srcEarth/test/F3/README.md
srcEarth/test/C1/README.md
srcEarth/test/C2/README.md
srcEarth/test/C3/README.md
srcEarth/test/C11/README.md
srcEarth/gridless/READ.ME
```

### C19 DIRECT_ACCESS trajectory-resolution and recurrence diagnostics (2026-08-19)

C19 keeps direct detector-folded `A(E,Omega)` as its primary observable and now treats
trajectory resolution as an explicit part of the science result.  The committed C19
input uses `MAX_TRACE_DISTANCE 0.0`; cumulative path length is therefore not the default
classifier and `MAX_TRACE_TIME` supplies the common physical trace-time budget at every
energy.  `run_C19.py` records the physical time implied by any requested finite path cap
in `C19_access_energy_grid.csv`.

Mode3D and GRIDLESS DIRECT_ACCESS files serialize per-trajectory termination evidence:
stable termination code, trace time/distance/steps, retries, mirror/bounce counters,
drift turns/angle, trap mechanism, and momentum spread.  The raw Tecplot headers publish
the code-to-name mapping.  Codes 0--8 retain their archived meaning and code 9 is
appended as `DRIFT_TRAPPED_FORBIDDEN`.

The new drift resolver is a **positive full-orbit recurrence test**, not
`timeout -> forbidden`.  It unwraps signed azimuth, forms azimuth-resolved profiles for
successive complete drift turns, and compares averaged `r`, `z/r`, and
`cos^2(pitch_angle)` with absolute+relative tolerances.  C19 defaults to three complete
turns, requires sufficient profile coverage and consecutive turn-to-turn recurrence,
checks outer-boundary margin and momentum spread, and only then returns the explicit
drift-trapped termination.  Full Lorentz trajectories remain authoritative; no
guiding-centre approximation is substituted in the penumbra.

`test/C19/run_C19_convergence.py` implements the required evidence sequence.  It first
runs distance and time sweeps with drift recurrence disabled, then adds a matched
recurrence-enabled case.  Optional timestep/mover sweeps provide numerical cross-checks.
The direct fold emits `C19_aperture_termination_budget.csv`, a compact response-weighted
per-head accounting of outer escape, inner loss, bounce trap, drift trap, and numerical
termination causes.

The Stage-A rigidity diagnostic is also restored.  `run_C19.py` now writes
`C19_access_classification_by_rigidity.csv` and per-case
`C19_access_classification_*.png` figures for every selected spacecraft/channel/epoch.
The figure uses the common DIRECT_ACCESS seed rigidities and shows EAST/WEST fractions
classified as allowed, physically forbidden, or unresolved; the secondary curve is the
normalized exact detector/spectrum interval weight.  Detailed termination causes remain
in the CSV.  This product is diagnostic only and is explicitly regression-tested so it
cannot disappear again during later packaging/refactoring.

The direct-derived equivalent-cutoff diagnostic is conservative: intervals touching
`UNRESOLVED` retain only blocked-area lower/upper bounds; no hidden `0.5*dR` midpoint is
created.  C19 distinguishes `INCONCLUSIVE_TRAJECTORY_RESOLUTION`,
`INCONCLUSIVE_DIRECT_BOUND_WIDTH`, and a genuinely resolved `MODEL_MISMATCH`.  Publication
runs can additionally require real detector attitude, an independent incident spectrum,
and calibrated response provenance.


### C19 response-weighted frozen-field validity guardrail (2026-08-21)

C19 traces particles through a magnetic-field snapshot that is frozen at the selected
observation epoch.  Long trajectories therefore carry an additional modelling
uncertainty: even when the Lorentz integration itself is numerically valid, a particle
that requires several minutes to escape or to establish a bounded orbit samples a real
magnetosphere that would have evolved during that interval.

The historical guardrail tested only the longest contributing trajectory,

```text
max(trace_time) > FROZEN_FIELD_WARNING_TIME  -> reject the complete direct scalar
```

which was too conservative for an aperture-integrated detector observable.  It ignored
the `J(E) G(E,Omega) dE dOmega` weight of the long trajectory, so one negligible
sky/energy sample could veto an otherwise well-resolved E/W ratio.  It also used a
warning time numerically equal to the common 300-s trace cap; an RK4 trajectory that
crossed the cap on its final finite step (for example 300.25 s for `DT_TRACE=0.25 s`)
could therefore trigger the physical guard solely because of integration-step
overshoot.

The current implementation replaces that single-trajectory veto with a
**response-weighted sensitivity assessment**.  For every DIRECT_ACCESS aperture fold,
`run_C19.py` apportions the exact interval integral of the assumed incident spectrum and
detector response between the two rigidity endpoints, multiplies it by the same
solid-angle/source-anisotropy factor used by the detector fold, and accumulates the
fraction of detector signal represented by trajectories longer than

```text
T_long = --frozen-field-warning-seconds
         + --frozen-field-time-tolerance-seconds .
```

The default tolerance is the AUTO sentinel `-1`, resolved to `0.5*DT_TRACE`; this keeps a
normal one-step trace-limit overshoot from being interpreted as new physical evidence.
The fold reports total long-trace and >2*T_warning fractions, response-weighted p50/p90/
p99 trace times, and partitions the long response into allowed, physically forbidden,
and still-unresolved populations.

The guardrail deliberately distinguishes three levels:

1. **static-field warning** -- any nonzero detector response comes from a trajectory
   beyond `T_long`.  This is provenance only and does not erase a resolved scalar.
2. **long-trace dominance warning** -- the long-duration response exceeds
   `--max-long-trace-response-fraction` (default 0.05).  This remains advisory because a
   positively resolved long trajectory is not equivalent to trajectory uncertainty.
3. **hard static-field guard** -- the response fraction that is both long-duration and
   still `UNRESOLVED` exceeds `--max-long-unresolved-response-fraction` (default 0.05),
   or an explicitly enabled conservative observable-sensitivity width exceeds
   `--max-static-field-ratio-bound-width-log10`.  The resulting status is
   `STATIC_FIELD_DOMINATED`.

The hard observable-sensitivity diagnostic is constructed without relabelling any
trajectory.  C19 recomputes conservative EAST/WEST signal bounds in which every energy
interval touching a long-duration trajectory is allowed to span `[0, full interval
signal]`.  These bounds are propagated to `static_field_log10_east_west_ratio_min/max`
and their width.  The width gate is disabled by default (`-1`) because an
experiment-independent publication tolerance has not yet been calibrated; it can be
enabled explicitly in convergence/publication studies.

This design preserves the core DIRECT_ACCESS rule: **timeout or long duration is never
converted to physical shielding**.  A long trajectory that is positively classified as
outer-boundary allowed, inner-boundary forbidden, or bounce/drift trapped contributes to
the warning provenance but does not by itself invalidate the scalar.  A long trajectory
that remains unresolved contributes to the hard uncertainty budget.

The implementation is auditable through `C19_aperture_availability.csv`,
`C19_aperture_termination_budget.csv`, and the ModelRow fields written to
`C19_comparison.csv`.  These products include long/very-long response fractions,
allowed/forbidden/unresolved long fractions, response-weighted trace-time percentiles,
static-field signal bounds, warning flags, and hard-guard status.  The dedicated
`C19_static_field_guardrail_<solver>_<field>.png` plot shows EAST/WEST long-response and
long+unresolved fractions together with the conservative static-field E/W bound width.
`C19_result.json` records aggregate counts and maximum observed fractions.

If a scientifically important fraction of the detector signal remains dependent on
long-duration trajectories, the preferred next physics step is not to keep extending a
frozen T05 snapshot indefinitely.  The guardrail is intended to identify precisely when
a time-dependent/interpolated magnetospheric background along the backward trajectory
becomes necessary.


See `test/C19/README.md` for the complete phased algorithm, parameter defaults, output
schema, convergence acceptance logic, and full AMPS regression requirements.

### C19 unresolved-only staged trajectory extension (2026-08-23)

C19 now has an optional unresolved-only convergence mechanism designed to reduce
`TIME_LIMIT`/`STEP_LIMIT` response without changing the scientific meaning of the test.
The primary trace still uses `CUTOFF_MAX_TRAJ_TIME`.  Only a sample unresolved at that
primary TIME/STEP budget is retraced, from the same initial phase-space state, using
successively larger total-time budgets controlled by
`CUTOFF_UNRESOLVED_EXTENSION_PASSES` and `CUTOFF_UNRESOLVED_EXTENSION_FACTOR`.
The C19 templates use 300 -> 600 -> 1200 s.  Resolved samples are not recomputed,
`DISTANCE_LIMIT` is not relaxed, and a sample still unresolved after the final pass
remains `UNRESOLVED` rather than being converted to shielding.

The solver scales the extended `MAX_STEPS` allowance with the larger time budget and the
observed mean integration step, preventing the numerical step ceiling from silently
replacing the requested time-convergence study.  New DIRECT_ACCESS provenance records
the primary termination/time, extension pass count, initial/final trace budgets, and the
mean radial change between the last compared drift-shell profiles.  Python
post-processing converts these into detector-response-weighted before/after fractions
and adds `C19_trace_extension_<solver>_<field>.png`.  Existing comparison, scatter,
parity, residual, transmission, directional-cutoff, boundary-spectrum, aperture,
access-classification, and static-field-guardrail plots remain separate and unchanged in
purpose.

The drift recurrence logic also accepts `TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE` as an
additional secular-drift veto.  It can only prevent a slowly migrating orbit from being
called drift trapped; it cannot generate a trapped/forbidden state by itself.  C19 uses
a finite value explicitly, while zero disables the extra gate globally.

This extension does not waive the frozen-field validity guardrail.  A trajectory that
needs 600--1200 s may be better classified numerically but still samples a real
magnetosphere over a duration for which a static T05 snapshot is increasingly
questionable.  Response-weighted long-trace diagnostics therefore remain active, and a
time-dependent field should be studied if scientifically important signal continues to
depend on very long trajectories.

# halo-laser-seam-2d

Upstream test: `epoch2d/tests/laser/input.deck`. Policy: `pointwise`.

## The test

One build of `epoch2d` runs the official 2-D laser deck of EPOCH on four MPI
ranks arranged 2 x 2, and the check grades the global field arrays EPOCH writes
into its SDF dumps.

The deck is a 500 x 500 grid over 20 x 20 micron with a continuous 1.06 micron
pulse entering at an angle of pi/8 through a `simple_laser` boundary at x_min,
`open` at x_max and periodic in y, run to 50 fs with a dump every 25 fs. There
are no particles, so the entire content of the run is the field advance plus
the exchange of guard cells between ranks. What this deck adds to the plain Yee
test is that a physical boundary and an interior rank seam sit on the same
axis: only the two ranks that own the global x_min edge apply the laser
condition, only the two that own x_max apply the open condition, and the ranks
on the other side of each seam must instead receive their x guard cells from a
neighbour. That is the case where a port most often confuses "there is a
neighbour rank here" with "there is a physical boundary here". The oblique
incidence puts a phase ramp along y, so the seam that runs along the
propagation direction also carries a non-trivial transverse profile rather than
a constant.

The deck is the upstream file with four changes: `nprocx` and `nprocy` are set
explicitly so the layout does not depend on how many cores the machine offers,
`use_pre_balance = F` with `balance_first = F` pin the partition to the one
`mpi_routines.F90` computes rather than the one the pre-run load balancer might
choose, `use_random_seed = F` and a large `stdout_frequency` keep the run
reproducible and the log short, and the `ex` and `bz` output lines that
upstream leaves commented out are enabled, so that all three field components
the 2-D solver advances are graded rather than Ey alone. Resolution and window
are the upstream values.

This check runs one deck and builds EPOCH itself. It was split out of a
combined 2-D halo check once the suite budget stopped counting build time (SPEC
revision 5.3), so that each official deck is one check; the Yee deck it used to
share a build with is now `halo-fdtd-2d`.

Runtime knobs (`run.sh --help`): `SAB_NX`, `SAB_TEND_SCALE`,
`SAB_DT_SNAPSHOT_SCALE`, `SAB_NPROCX`, `SAB_NPROCY`, `SAB_PRE_BALANCE` and
`SAB_MAKE_JOBS`. The defaults are the graded values and are what the reference
run uses. `SAB_TEND_SCALE` scales the control block's `t_end` alone and
`SAB_DT_SNAPSHOT_SCALE` the output block's `dt_snapshot` alone, so the upstream
window and the upstream dump cadence can each be restored without disturbing
the other; note that changing the cadence moves the graded dump indices. The
build dominates the wall time and is reported separately as
`SAB_BUILD_SECONDS`.

## The two initial conditions

`ic/nominal` is the deck described above. `ic/variant` is the same deck with
the laser intensity multiplied by `(1 + 1e-15)`, nine units in the last place
of the binary64 the dumps are written in. That is far too small to be physics
and far too large to be lost to rounding, so it changes the round-off path of
the whole run and the distance between the two runs measures the floor of this
pass policy.

The obvious variant for a decomposition check would be a different rank layout,
and it was tried: the same build at 2 x 2 and at 4 x 1 produces bit-identical
graded fields, because a field halo exchange moves data and does no arithmetic.
That is a strong statement about the module, and revision 6 makes it an
executable graded condition rather than a remark: `layout-invariance-2d` runs
an official deck on one rank and on its graded layout inside a single
invocation and grades the difference between the two assembled fields. As a
variant it would still measure nothing, because it would leave the two runs
identical, so the perturbation below is used instead. The layout stays
reachable through `SAB_NPROCX` and `SAB_NPROCY`.

`run.sh altbuild` runs `ic/nominal` on the same pinned source built with
EPOCH's own debug profile (`make -C epoch2d COMPILER=gfortran MODE=debug`: `-O0
-g` instead of the default `-O3`, full warnings promoted to errors,
`-ffpe-trap=invalid,zero,overflow` and `-fbounds-check` turned on, and
`-DPARSER_CHECKING -DDECK_DEBUG` compiled in) instead of the default build;
grading never uses it, while self-validation measures the check's floor between
the two legitimate builds from it.

## The pass policy

Every graded value is compared with the reference under an absolute bound: 100
V/m on the electric field components, 1e-06 T on Bz, and exact equality on the
integer partition ladder that EPOCH writes into every dump. The bounds differ
because the floors differ: Bz is smaller than E by a factor of the speed of
light and its round-off is smaller in the same proportion, and the partition
ladder is a list of integers computed by an exact rule, so anything but
equality there is a wrong decomposition.

The bound is physical because this deck has no particles: if the fields come
out right, the halo exchange and the boundary dispatch were both right, and if
either is wrong the error is not subtle. Applying the laser or the open
condition on a rank that has a neighbour, or exchanging all faces at once from
one snapshot and so leaving the diagonal corner blocks stale, or handing the
remainder cells to the wrong end of an axis, moves field values at and near the
seam by a substantial fraction of the pulse itself, around 1e10 V/m.

The bound is achievable because the field update is an elementwise stencil with
no summation in it, so the same code on the same decomposition reproduces
itself exactly, and the measurements below show how far two legitimate runs
actually sit apart. The bound is left several orders of magnitude above that
floor on purpose, so that a port whose arithmetic is reassociated on an
accelerator is not failed for being an accelerator.

## The upstream deck

`upstream/input.deck` is a byte-for-byte copy of
`epoch2d/tests/laser/input.deck` as the pinned source ships it, and
`upstream/nominal.patch` is the complete unified diff between that file and
`ic/nominal/input.deck`. Nothing in the deck pair is an undocumented
adaptation: every line of that patch is one of

- the explicit `nprocx`/`nprocy` layout, written from the `SAB_NPROC*` knobs,
  and the balancer pins, written from `SAB_PRE_BALANCE`;
- the grid size and the window, which `run.sh` rewrites from `SAB_NX`,
  `SAB_TEND_SCALE` and `SAB_DT_SNAPSHOT_SCALE` -- the last two act on `t_end`
  and on `dt_snapshot` independently, so the upstream window and the upstream
  dump cadence can each be restored without disturbing the other;
- `use_random_seed = F`, which restates EPOCH's own default, and a large
  `stdout_frequency`, which only shortens the log;
- output lines: the blocks the upstream file leaves commented out and this
  check has to enable in order to grade anything at all, and, where the deck
  has one, the distribution-function output this check does not grade.

`run.sh --help` lists every setting, and the defaults leave `ic/nominal` at the
values it ships -- the two scale knobs at 1 rewrite `75 * femto` as `75.0 *
femto`, the same number -- so the difference from upstream is both visible and
reversible.

## Policy under revision 5.6.0

Policy under SPEC revision 5.6.0: pointwise. The graded run is deterministic at
a fixed layout -- the field advance is an elementwise stencil with no reduction
in it -- so a bound exists that contains the measured sensitivity over the
graded window and still rejects a real fault by seven to eight orders of
magnitude, which is the condition 5.6.0 sets for preferring pointwise. None of
the invariants cases applies: no random stream drives the run and there are no
particles in the deck to amplify rounding. The one discrete graded array, the
integer rank partition ladder, is compared at atol 0. Revision 5.6.0 lists "an
output whose values are discrete, a bin index or a switch, where a small change
flips the value outright" among the invariants cases, and this is deliberately
not that case: the ladder is not a discretisation of a continuous quantity that
a rounding difference could tip across a bin edge but the output of an exact
integer remainder rule applied to integer input, with no arithmetic on it that
rounding can reach. It is exact by construction, and the shipped
self-validation record confirms it -- every ladder in the leaf came back with
max_abs_error exactly 0.0 and values_over_bound 0 under the variant. Zero
tolerance is therefore the bound that contains the measured sensitivity.

## Evidence

All numbers below are native measurements on the packaging host (gfortran 15,
OpenMPI 5, four ranks, `--oversubscribe --bind-to none`); the in-container
spread is measured later by the self-validation run and recorded in the rubric.

- Two legitimate builds of the pinned source, the stock `-O3` gfortran flags
  against the same tree with `-O2`: every graded array bit-identical, largest
  absolute difference exactly zero.
- The variant preview, the `-O3` build on `ic/variant` against `ic/nominal`:
  largest absolute difference 6.1e-04 V/m on Ex, 6.0e-04 V/m on Ey, 1.8e-12 T
  on Bz, and zero on the partition ladder.
- Decomposition invariance: the same build at 2 x 2 and at 4 x 1 ranks agrees
  bit for bit on every graded field value; only the partition ladder changes,
  as it must.

The complete graded-default x86 calibration selfcheck finished
2026-09-04T13:49:13Z: its own nominal-versus-variant distance was 0.000747681.
All 7 graded arrays (1500002 values) contained the measured sensitivity under
their own bounds, with 0 values over bound; the worst array was ey_0002.f64 at
7.47681e-06 of its bound. This comparison measures nominal-variant
sensitivity, not a same-input run/build floor.

The nominal run took 1.8 s excluding its 58.0 s build, and expected_runtime_s
is 3 s. The earlier independent same-input build-floor evidence above remains
distinct.

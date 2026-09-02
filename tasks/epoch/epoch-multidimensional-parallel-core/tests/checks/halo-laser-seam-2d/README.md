# halo-laser-seam-2d

Upstream test: `epoch2d/tests/laser/input.deck`. Policy: `pointwise`.

## The test

One build of `epoch2d` runs the official 2-D laser deck of EPOCH on four MPI
ranks arranged 2 x 2, and the check grades the global field arrays EPOCH writes
into its SDF dumps.

The deck is a 500 x 500 grid over 20 x 20 micron with a continuous 1.06 micron
pulse entering at an angle of pi/8 through a `simple_laser` boundary at x_min,
`open` at x_max and periodic in y, run to 50 fs with a dump every 25 fs. There
are no particles, so the entire content of the run is the field advance plus the
exchange of guard cells between ranks. What this deck adds to the plain Yee test
is that a physical boundary and an interior rank seam sit on the same axis: only
the two ranks that own the global x_min edge apply the laser condition, only the
two that own x_max apply the open condition, and the ranks on the other side of
each seam must instead receive their x guard cells from a neighbour. That is the
case where a port most often confuses "there is a neighbour rank here" with
"there is a physical boundary here". The oblique incidence puts a phase ramp
along y, so the seam that runs along the propagation direction also carries a
non-trivial transverse profile rather than a constant.

The deck is the upstream file with four changes: `nprocx` and `nprocy` are set
explicitly so the layout does not depend on how many cores the machine offers,
`use_pre_balance = F` with `balance_first = F` pin the partition to the one
`mpi_routines.F90` computes rather than the one the pre-run load balancer might
choose, `use_random_seed = F` and a large `stdout_frequency` keep the run
reproducible and the log short, and the `ex` and `bz` output lines that upstream
leaves commented out are enabled, so that all three field components the 2-D
solver advances are graded rather than Ey alone. Resolution and window are the
upstream values.

This check runs one deck and builds EPOCH itself. It was split out of a combined
2-D halo check once the suite budget stopped counting build time (SPEC revision
5.3), so that each official deck is one check; the Yee deck it used to share a
build with is now `halo-fdtd-2d`.

Runtime knobs (`run.sh --help`): `SAB_NX` sets the cell count along x (`ny` is
`nx`), `SAB_TEND_SCALE` multiplies both the end time and the dump interval so the
graded dump indices do not move, `SAB_NPROCX` and `SAB_NPROCY` set the rank
layout, and `SAB_MAKE_JOBS` sets the build parallelism. The defaults are the
graded values and are also the upstream resolution and window. On four ranks the
deck itself takes about a second; the build dominates the wall time and is
reported separately as `SAB_BUILD_SECONDS`.

## The two initial conditions

`ic/nominal` is the deck described above. `ic/variant` is the same deck with the
laser intensity multiplied by `(1 + 1e-15)`, about two units in the last place of
the binary64 the dumps are written in. That is far too small to be physics and
far too large to be lost to rounding, so it changes the round-off path of the
whole run and the distance between the two runs measures the floor of this pass
policy.

The obvious variant for a decomposition check would be a different rank layout,
and it was tried: the same build at 2 x 2 and at 4 x 1 produces bit-identical
graded fields, because a field halo exchange moves data and does no arithmetic.
That is a strong statement about the module and it is recorded in the rubric's
evidence, but as a variant it would leave the two runs identical and measure
nothing, so the two-ulp perturbation is used instead. The layout stays reachable
through `SAB_NPROCX` and `SAB_NPROCY`.

## The pass policy

Every graded value is compared with the reference under an absolute bound:
100 V/m on the electric field components, 1e-06 T on Bz, and exact equality on
the integer partition ladder that EPOCH writes into every dump. The bounds differ
because the floors differ: Bz is smaller than E by a factor of the speed of light
and its round-off is smaller in the same proportion, and the partition ladder is
a list of integers computed by an exact rule, so anything but equality there is a
wrong decomposition.

The bound is physical because this deck has no particles: if the fields come out
right, the halo exchange and the boundary dispatch were both right, and if either
is wrong the error is not subtle. Applying the laser or the open condition on a
rank that has a neighbour, or exchanging all faces at once from one snapshot and
so leaving the diagonal corner blocks stale, or handing the remainder cells to
the wrong end of an axis, moves field values at and near the seam by a
substantial fraction of the pulse itself, around 1e10 V/m.

The bound is achievable because the field update is an elementwise stencil with
no summation in it, so the same code on the same decomposition reproduces itself
exactly, and the measurements below show how far two legitimate runs actually sit
apart. The bound is left several orders of magnitude above that floor on purpose,
so that a port whose arithmetic is reassociated on an accelerator is not failed
for being an accelerator.

## Evidence

All numbers below are native measurements on the packaging host (gfortran 15,
OpenMPI 5, four ranks, `--oversubscribe --bind-to none`); the in-container spread
is measured later by the self-validation run and recorded in the rubric.

- Two legitimate builds of the pinned source, the stock `-O3` gfortran flags
  against the same tree with `-O2`: every graded array bit-identical, largest
  absolute difference exactly zero.
- The variant preview, the `-O3` build on `ic/variant` against `ic/nominal`:
  largest absolute difference 6.1e-04 V/m on Ex, 6.0e-04 V/m on Ey, 1.8e-12 T on
  Bz, and zero on the partition ladder.
- Decomposition invariance: the same build at 2 x 2 and at 4 x 1 ranks agrees bit
  for bit on every graded field value; only the partition ladder changes, as it
  must.
- Peak magnitudes over the graded window are around 9e10 V/m for Ey, 4e10 V/m for
  Ex and 3e2 T for Bz, so the bounds are about one part in 1e9 of the signal.

Policy, bounds, window and variant are proposals until the curator finalizes them
after the calibration self-validation run.

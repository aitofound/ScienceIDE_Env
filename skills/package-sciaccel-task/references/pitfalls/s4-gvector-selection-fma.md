# S4: the retained Fourier basis depends on the build

**Symptom.** Two legitimate builds of the same pinned S4 source print a
different `GetNumG()` and different diffraction efficiencies on the same deck,
by amounts no tolerance covers. Rows keyed by G index come out permuted even
where the values agree.

**What breaks.** `S4/gsel.c` ranks candidate reciprocal-lattice vectors by a
floating-point comparator (a sum of three products of exact integers with
lattice constants) and sorts them with the glibc quicksort copied into
`S4/sort.c`, which is not stable. The path every deck takes by default is the
circular truncation, `Gsel_circular`, whose sort is at `gsel.c:147` and whose
tie test is `G_same` (`gsel.c:74-90`, within `2*DBL_EPSILON*maxlen`); the
parallelogramic branch (`Gsel_parallelogramic`, sort at `:111`) is not
reached unless a deck asks for it. Vectors on the same |G| shell compare as a
tiny nonzero remainder whose sign depends on rounding. The circular truncation
then walks back across tied vectors at the boundary, so when the remainders
change, a different NUMBER of vectors survives. The two builds solve different
truncations of the same Fourier series; neither approximates the other.

**Mechanism.** FMA contraction. With the flags the leaf pins (no `-march`),
gcc 12 on arm64 emits two fused multiply-adds in the comparator at `-O2` and
none at `-O0`; gcc 13 on x86_64 emits none at either level. The `-O0`
altbuild therefore exposes the instability on an arm64 host and is
bit-identical on the x86 worker. Any GPU port fuses by default, so a port
trips it on every host.

**Measured** (arm64 Docker, `-O0` versus `-O2`, same LAPACK):

| deck | effect |
|---|---|
| `2d/Li_JOSA_14_2758_1997/ex2.lua` | `GetNumG()` 81 vs 77 and 160 vs 159; max error 4.0 |
| `patterns/nonorth.lua` | 8088 of 16384 unit-cell values and 44211 of 90000 map values move by 0.21; sorted sets still differ by 0.202 |
| `2d/Li_JOSA_14_2758_1997/ex3.lua` | 69 of 98 rows permuted; every efficiency matched by its own G index agrees to 3.0e-16 |

`-DHAVE_LAPACK` alone moved ex2 by 1.7e-13 and left nonorth bit-identical.

Two more measurements from #504, a `linux/amd64` build of the task image
against the arm64 one: the seven checks that never call LAPACK read an
altbuild floor of exactly 0 on x86 and nonzero on arm64, while the fourteen
that do call it hold their order of magnitude on both. Whether a check calls
LAPACK predicts its x86 floor better than the architecture alone does; that
is the same point as [altbuild-floors-are-host-specific](altbuild-floors-are-host-specific.md),
with a sharper predictor. And the instability reaches graded output through
a printed integer, not only through the physics: three checks in #504 graded
`S:GetNumG()` as a column and had to stop, since it fails on bookkeeping
rather than on a wrong answer.

**How to detect it.** Any code that sorts a discrete basis,
mesh or particle set by a floating-point key and then truncates. Look for a
`qsort` or hand-rolled sort over doubles followed by `N` kept. Test by
building once with FMA on (`-O2` on arm64, or `-mfma -ffp-contract=fast` on
x86) and once off, and diff the count kept, not only the values. Before
calibrating, grep the decks for the retained count (`GetNumG` in S4) and take
it out of the graded columns; a printed integer that moves by one fails every
bound. A moving front that meets nodes at exact
ties is the same mechanism with a comparison instead of a sort:
[eprem-shock-front-node-tie](eprem-shock-front-node-tie.md).

**What to do in the check.** A check that prints G indices or whose lattice
has a degenerate shell at the truncation boundary is not gradeable at this
pin; record it unsuitable with the measurement. Where only the order moves
and the set is identical, compare by G index in `validate.py` and still fail a
candidate that emits a different set. Do not widen a bound to cover it, and do
not patch the vendored source.

**Where measured.** aitofound/ScienceAccelBench issue #505 and PR #504
(`tasks/s4/fmm-fourier-factorization`), 2026-09-06; the LAPACK split, the
printed count and the line references from issue #594 on the same PR,
2026-09-08.

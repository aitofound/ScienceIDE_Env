# EPREM: the ideal shock meets every third grid node at an exact tie

**Symptom.** Two legitimate builds of the same pinned EPREM source (`-O3` and
`-O1`, same gcc, same host) agree to 1e-15 on the bulk of the particle flux
in the official `shock.cfg` deck but differ by up to 7% in the accelerated
tail and by 5% at the point observers, while a two-ULP input perturbation
moves the same arrays by 3e-14. A pointwise bound that passes the variant
by 1500x fails the alternative build by 1e9x.

**What breaks.** Whether a node is inside the shock is a yes/no test once per
time step, `flow.c:96`: `radpos.r <= shockSpeed*shockTime + rScale`. Nodes
that pass it get the shocked density and field from `idealShockFactor`
(`flow.c:585-604`, gated in `mhdDensity` at `flow.c:218-228`); nodes that fail
it get the ambient wind. The grid is seeded by running the clock back
`TOTAL_NUM_SHELLS * tDel` (`simCore.c:65`) and spawning one shell per step
that then rides the wind at `flowMag`, so at the shock's start the initial
nodes sit one wind-step apart. The official deck sets `idealShockSpeed` to
1200 km/s and `flowMag` to 300 km/s, a ratio of exactly 4. Node `j` therefore
reaches the front at step `n = j/3`: for every `j` divisible by 3 the node's
radius and the front's radius are equal in exact arithmetic at a step
boundary, and the last bit decides which side of the test it lands on. The
two builds round differently because gcc 14 on arm64 contracts multiply-adds
at `-O3` and not at `-O1` (measured in the image: 35 fused instructions in
`flow.c`, 39 in `geometry.c`, 14 in `simCore.c`, 222 in
`energeticParticles.c` at `-O3`; zero in each at `-O1`). A node that lands on
the wrong side enters the shock one step late, sees one step less of
acceleration, and the difference spreads along the stream through the
transport operators. The tail above 10 MeV comes from the few nodes at the
front, so a handful of late nodes moves it by tens of percent.

**Measured** (arm64 Docker, gcc 14.2, `-O1` altbuild against the `-O3`
reference, 24 streams by 500 nodes, `tDel` 0.01 day, 1 day):

| deck | nodes that entered the shock one step late | where |
|---|---|---|
| official `shock.cfg` (90-degree cone) | 12 of 12,000, indices 105, 108, 117, 123, 129, 144, 156 | four streams on faces 2 and 5 |
| same deck with `idealShockWidth=0` (spherical) | 75 of 12,000, 14 distinct indices, every one a multiple of 3 | all 24 streams |
| same deck with `idealShockSpeed=1210` km/s (ratio 4.033, the tie broken) | 0 of 12,000; whole flux array within 2e-16 of the peak, tail cells within 2.4e-13 relative | none |

| graded quantity, official deck | `-O1` versus `-O3` | two-ULP `lamo` variant |
|---|---|---|
| stream flux, cells above 1e-3 of the peak | 1.3e-13 | 3e-14 |
| stream flux, cells at 1e-6 to 1e-3 of the peak | up to 7.3e-2 | 3e-14 |
| observer flux, cells above 1e-3 of its peak | up to 5.2e-2 | 6e-15 |
| mean free path at a late node | 6.6e-3 (the value of the node one step behind) | 6.5e-16 |
| spectrum summed over all 12,000 nodes, bins below 1 MeV | 6e-7 to 4e-3 | 7e-15 |
| same spectrum, bins above 10 MeV | 3e-2 rising to 2.3e-1 | 7e-15 |
| slope of that spectrum above 10 MeV | 4.6e-3 | 0 |
| total intensity per stream (24 values) | 4.4e-4 | 0 |
| total intensity per observer (4 values) | 2.7e-3 | 0 |

**How to detect it.** Any Lagrangian or moving-front code where a front
position and a node position are both linear in the step count and the deck
makes their speeds commensurate: grep for the membership test (`<=` or `<`
against a position that advances by `speed * time`) and divide the two speeds
in the deck. Then build twice (FMA on and off; on arm64 that is `-O3` against
`-O1`, on x86 `-O2 -mfma -ffp-contract=fast` against the pinned build) and
look for shifted values at node indices with a common divisor. A two-ULP
input variant does not find this: it moves both sides of the tie together. The
same contraction difference is behind [s4-gvector-selection-fma](s4-gvector-selection-fma.md)
(a sort on a floating-point key), and [altbuild-floors-are-host-specific](altbuild-floors-are-host-specific.md)
says why an x86 `-O0` altbuild may not expose it.

**What to do in the check.** Do not widen a pointwise bound to cover it, and
do not patch the vendored source. Grade the deck by invariants that sum over
many nodes: per-stream and per-observer total intensity, the low-energy
spectrum summed over all nodes, and the slope of the accelerated tail, each
bounded at about ten times its measured altbuild floor. Individual tail bins
and per-node values on a shock deck are not gradeable at this pin. A custom
deck avoids the tie by choosing a shock speed whose ratio to the wind speed,
in lowest terms `p/q`, has `q` larger than the number of steps in the run
(node `j` meets the front at step `n = q j/(p-q)`, an integer only when `q`
divides `n`); 1201 km/s against 300 km/s (`q = 300`) has no tie in a
100-step run, and the 1210 km/s measurement above shows what breaking the tie
buys. Measure the new deck on both builds before trusting it.

**Where measured.** aitofound/ScienceAccelBench PR #518
(`tasks/open-eprem/focused-particle-transport`), curator's revision on the
author's branch, 2026-09-08; issue #576 on the same repository.

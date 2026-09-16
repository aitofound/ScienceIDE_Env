# Calibration reading priorities

The following rows have less than 50 times headroom against the measured input perturbation or alternative-build difference. This is the pipeline reading-order flag, not an additional pass rule or a proposed tolerance change. The tolerance includes all explicit observation overrides and field-scale terms. The curator decides scientific equivalence.

| Check | Input margin | Alternative-build margin |
|---|---:|---:|
| deck-linear-elasticity-truss-bridge3d | 5.44x | identical graded values |
| deck-navier-stokes-stokes-slip-bc | 6.88x | identical graded values |
| deck-linear-elasticity-shell10x-cantilever | 14.3x | identical graded values |
| example-linear-elastic-up | 446x | 20.4x |
| deck-acoustics-vibro-acoustic3d | 21.4x | identical graded values |
| deck-multi-physics-piezo-elastodynamic | 32x | identical graded values |
| deck-linear-elasticity-seismic-load | 32.7x | identical graded values |
| deck-multi-physics-piezo-elasticity-macro | 1.14e+09x | 41.2x |
| elastodynamic-solvers | 41.9x | identical graded values |
| deck-homogenization-linear-homogenization-up | 93.7x | 42.6x |

107 input-comparison rows have margins above 10,000x. The proposed scientific tolerances are not mechanically reduced to two-ulp spreads; all 158 validators reject the recorded percent-level output fault. This mutation evidence is a bounded check of the validator, not a proof against every possible incorrect physical implementation.

146 alternative-build comparisons have identical graded values. Both actual CMake flag sets are recorded in `toolchain-evidence.json`; identical numerical output in a check is visible rather than disguised by an ungraded sidecar. A zero measured floor on this host does not establish portability across every compiler or optional backend.

The complete CLI-generated presentation and warrants are in `review-brief.md`; complete additional tolerance terms are in `calibration-bounds.md`. No input-perturbation pair is identical in every graded value.

# layout-invariance-2d

## Physical contract

`run.sh` produces SDF snapshots at dumps 0002, 0003. The extractor reads assembled global plain-variable blocks and writes raw little-endian arrays in physical coordinate order (x fastest, then y, then z). The validator compares the same coordinate in reference and candidate; it never compares rank slots or particle ownership.

**Graded observables:**
- Ex, Ey and Bz global grids for serial and decomposed runs, plus their decomposed-minus-serial delta (the delta must be within the explicit zero bound).
- Total field energy, using `0.5*epsilon0*integral(|E|^2 + c^2|B|^2)dV`, in joules.

## Bounds and limitations

Coordinate-indexed fields use the per-file rtol with the accepted effective atol floor of `0.0025`, calibrated from the fresh identical-physics MPI-layout run; scalar energies retain their per-file bounds. Decomposed-minus-serial field deltas use the accepted `0.001` zero floor. The user accepted these measured bounds on 2026-09-12. Missing, empty, shape-mismatched, or non-finite data fails closed.

## Deliberately ungraded or unavailable

Rank identifiers, rank boundaries, per-rank counts, particle ownership/order, load-balancing ladder/repartition count/timing, and other implementation bookkeeping are diagnostics only and do not appear in the rubric. Particle records emitted solely to expose global counts are likewise ungraded. Components not requested by this deck (for example Jy/Jz/Ez/Bx/By) are not invented. Total charge and momentum remain ungraded because these decks do not emit a validated scalar diagnostic.

Layout note: serial and decomposed global meshes are compared by shape and canonical physical coordinate; `delta_*` is independently checked against zero.

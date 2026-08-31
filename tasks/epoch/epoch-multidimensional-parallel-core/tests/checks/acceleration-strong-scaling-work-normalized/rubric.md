# Rubric: PAR-19 (acceleration-strong-scaling-work-normalized)

## Pass policy

Two bounded, source-grounded, non-timing invariants, both owner-chosen and
recorded in `run.json`: (1) no rank's cell count may exceed
`balance_bound_factor` (1.2) times the ideal `nx*ny/nproc` share, bounding
load-imbalance quality; (2) going from 1 to N ranks, the observed shrinkage of
the busiest rank's cell count must be at least 0.6x the ideal Nx shrinkage
(a loose bound that only fails if the decomposition is badly degenerate,
e.g. duplicating work instead of partitioning it). `elapsed_seconds` is
recorded for every run as supplementary evidence only; it is never compared
against a threshold, per the frozen contract's ban on flaky wall-clock
claims. See comment/README.md for the remote-calibration plan for this row.

## Owner

Draft leaf; tolerances above are the packaging author's provisional,
documented policy pending the science owner's sign-off (see the packaging
skill's curation step). They are not copied from any other leaf's tolerances.

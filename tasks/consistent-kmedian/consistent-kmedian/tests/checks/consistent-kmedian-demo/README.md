# consistent-kmedian-demo

Upstream test: `code/consistent-kmedian/paper1_consistent_kmedian.py`. Policy: `pointwise`.

## The test

`run.sh` calls `driver.py`, which parametrizes the upstream `__main__` demo:
it constructs a 1-D metric over 40 points (facility/client ids `0..39`, each
at a fixed position, positions given by `ic/<condition>/config.json`'s
`coords` list) and feeds the points one at a time into
`OnlineConsistentKMedian.add_point`, in id order, with `k=3`, `z=2`,
`gamma=1.0`, `eps=0.3`. It writes the algorithm's state at the same six
checkpoints the upstream demo prints (`t=0,8,16,24,32,39`) plus the final
median set. There are no runtime knobs: the whole configuration is fixed by
the initial condition, and the run completes in well under a second on one
core.

Graded output files (written to the check's output root):
- `trace.txt`: one line per checkpoint, three whitespace-separated columns:
  `total_recourse approx_cost p` (in `%.17g` text, full double precision).
- `medians.txt`: the final median facility ids, sorted ascending, one per
  line (`%.17g` text; always exactly `k=3` lines for this configuration).

## The two initial conditions

`ic/nominal/config.json` reproduces the upstream demo's own inputs exactly
(the same seeded coordinate construction, `random.Random(0).uniform(0,100)`
for 40 points, recorded literally as the `coords` list so the initial
condition is self-contained). `ic/variant/config.json` is byte-identical
except `coords[0]` is perturbed by two ULPs at binary64 precision
(`84.4421851525048` -> `84.44218515250483`). No alternative build is
declared (`altbuild`): the module is pure-Python and stdlib-only, so there
is no compiler flag or second interpreter build that would legitimately
differ.

## The pass policy

Every graded value in `trace.txt` and `medians.txt` is compared with
`|candidate - reference| <= atol + rtol*|reference|`, `atol=1e-12`,
`rtol=3e-13`. `total_recourse` and the final median set are the paper's
consistency guarantee and the algorithm's output respectively: a wrong
penalty formula, a broken swap-efficiency threshold, or a wrong
outlier-budget doubling changes them by an order of magnitude or more, far
above this bound. `approx_cost` is the true (unpenalized) k-median-with-
outliers cost of the current median set; the bound sits about three orders
of magnitude above the measured two-ULP floor (see Evidence), leaving room
for the summation-order differences a differently structured but faithful
port could introduce over `cost_p`'s ~40-term sum
(`paper1_consistent_kmedian.py:60-67`).

## Evidence

Native (pre-Docker) calibration: `driver.py` run on `ic/nominal` and
`ic/variant` on this machine (Python 3.9.6), outputs diffed directly.
`total_recourse`, `p` and `medians.txt` were byte-identical between the two
runs; `approx_cost` differed only at the last two checkpoints, by at most
`2.842e-14` absolute (`4.68e-16` relative) -- this is the floor recorded in
`rubric.json`. `selfcheck`'s Docker-based calibration run will record the
real `self_validation_spread` / `self_validation_bound_fraction`, to be
folded back into this rubric at STOP 4.

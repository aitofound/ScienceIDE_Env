# db-gst-t-rounding-demo

Upstream test: `code/degree-bounded-steiner/paper2_degree_bounded_steiner.py`. Policy: `pointwise`.

## The test

`run.sh` runs `driver.py`, which rebuilds a fixed 30-node tree instance
(parent map, per-vertex cost, per-vertex degree bound, 5 leaf groups) from
`ic/<ic>/config.json` -- recorded literally, the same values the upstream
`random.seed(3)` construction produces, so the instance itself is the
initial condition -- and calls `DBGSTSolver(inst, eps_M_boost=8,
seed=7).solve()`, the same call the upstream `__main__` part (B) demo
makes. This runs the full Section 6 pipeline end to end: solve the LP
relaxation (7)-(12) via `scipy.optimize.linprog(method="highs")`, round
the LP solution to non-positive powers of 2, rescale by a level-dependent
threshold `gamma`, then repeat recursive randomized rounding `M=8` times
and union the results. There is no runtime knob: the configuration is
fixed and the check runs in well under a second on 1 core.

## The two initial conditions

`ic/nominal` is the 30-node instance above. `ic/variant` perturbs one
vertex's cost (`cost["n1"]`) by two ULPs at binary64 precision
(`3.3705636425086625` -> `3.3705636425086634`, delta `8.88e-16`); every
other input is byte-identical. The perturbed cost feeds directly into the
LP objective (`_solve_lp`, `paper2_degree_bounded_steiner.py:177`), so it
is an active input the pass policy can be calibrated against. Two discrete
branch points in this module were flagged as worth watching under
perturbation (module cut hazards / test-survey `why`): the power-of-2
rounding's `math.ceil(math.log2(val))` boundary, and `_rescale`'s strict
`x[v] < x[u]` level comparison. Neither flipped in the native calibration
run at this instance's scale (see Evidence) -- only the continuous
`cost`/`lp_value` observables moved.

## The pass policy

`pointwise`, `atol=1e-11`, `rtol=3e-13`. Graded files: `metrics.txt`
(`cost`, `lp_value`, `max_degree_violation_factor`,
`groups_covered`, `groups_total`) and `chosen_set.txt` (the chosen vertex
set `V`, sorted by vertex id -- an unordered collection put in the order
of its own identity, per the skill's rule for pointwise-graded
collections). `cost` and `lp_value` are the paper's headline observables
for Theorem 1.2 (Section 6): their ratio is what the theorem bounds by
`O(log n log k)`, and `max_degree_violation_factor` is the other half of
the bicriteria guarantee (`O(log n)`). A wrong LP construction, a broken
power-of-2 modification or rescaling, or a broken recursive rounding
changes these by an order of magnitude or more, or changes which vertices
are chosen; the bound sits three to four orders of magnitude above the
measured two-ULP floor, tight enough to catch such a fault immediately
while tolerating the LP solver's own summation-order noise.

## Evidence

Native (non-Docker) run of `driver.py` on `ic/nominal` vs `ic/variant`,
this machine, Python 3.13.7 (scratch venv, numpy 2.5.3, scipy 1.18.1).
Determinism: two independent runs on `ic/nominal` produced byte-identical
output files. Nominal-vs-variant spread: `cost`/`lp_value` differ by
`7.105e-15` absolute / `1.691e-16` relative (both quantities are equal at
this LP-tight instance: `lp_value = cost = 42.010146110345175` nominal,
`42.01014611034518` variant); `max_degree_violation_factor` (`1.0`),
`groups_covered`/`groups_total` (`5`/`5`) and `chosen_set.txt` were
byte-identical between the two runs -- neither discrete branch point
flipped under this two-ULP perturbation. This is the pre-Docker
calibration measurement; STOP 4 revises these numbers from the Docker
selfcheck's `self_validation_spread` once it runs.

# degree-bounded-steiner: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

Whole-codebase module: owns the entire source root,
`paper2_degree_bounded_steiner.py`, implementing two self-contained,
polynomial-time results from "On Approximating Degree-Bounded Network
Design Problems" (Guo, Kortsarz, Laekhanukit, Li, Vaz, Xian,
arXiv:1907.11404) -- Lemma 2.1's balanced tree partitioning and Theorem
1.2's DB-GST-T bicriteria LP-rounding approximation. Nothing was excluded
from the module scope (single file, `paths: ["."]`); the one thing
deliberately not implemented by the source itself is Theorem 1.1 (Degree-
Bounded Directed Steiner Tree via state trees and a large LP over a
super-tree T-circ) -- the module docstring states it is a
quasi-polynomial-time construction not practical to reproduce as runnable
code, so there is nothing to package for it. The two checks cover the two
implemented parts independently: `balanced-tree-partitioning-demo`
(Lemma 2.1) and `db-gst-t-rounding-demo` (Theorem 1.2); part (A)'s code is
not called by part (B) at runtime, so the two checks are genuinely
independent rather than pipeline stages of each other.

## Build

Pure-Python, numpy/scipy as installed runtime dependencies (no compiled
extension of this source): nothing is compiled. `run.sh` reports
`SAB_BUILD_SECONDS=0` unconditionally for both checks; there is nothing
for a later check to reuse and nothing to build in the first place.

## Tolerances

Both checks' floors were measured natively (pre-Docker, this machine,
Python 3.13.7, scratch venv, numpy 2.5.3, scipy 1.18.1), not yet from a
Docker `selfcheck`. `balanced-tree-partitioning-demo` has no continuous
input to perturb (the tree topology is entirely discrete and every graded
value is an exact integer), so `ic/variant` is byte-identical to
`ic/nominal` by construction and the policy is `atol=0`/`rtol=0` (exact
match) -- there is no floor to sit above, since the only source of
disagreement is a real implementation fault. `db-gst-t-rounding-demo`
perturbs `cost["n1"]` by two ULPs; `driver.py` on `ic/nominal` vs
`ic/variant` differed by at most 7.105e-15 absolute / 1.691e-16 relative in
`cost`/`lp_value`, while the chosen set, `max_degree_violation_factor` and
`groups_covered`/`groups_total` came back byte-identical -- the two
discrete branch points flagged during the survey (the power-of-2 rounding's
`math.ceil(math.log2(val))` boundary, and `_rescale`'s strict
`x[v] < x[u]` comparison) did not flip at this instance's scale.
`rubric.json`'s `atol=1e-11`/`rtol=3e-13` sits three to four orders of
magnitude above that floor. The Docker `selfcheck` (nominal vs. variant,
this machine) reproduced these exact numbers: `balanced-tree-partitioning-demo`
`self_validation_spread = 0.0` (flagged `identical`, as the rubric
declares and expects); `db-gst-t-rounding-demo`
`self_validation_spread = 7.105427357601002e-15`,
`bound_fraction = 0.000314` -- about 3181x headroom.

STOP 4 (2026-09-14): the human reviewed the Docker-measured spreads and
margins above and confirmed both checks' tolerances as final:
`atol=0`/`rtol=0` (exact match) for `balanced-tree-partitioning-demo`, and
`atol=1e-11`/`rtol=3e-13` for `db-gst-t-rounding-demo`.

## Blind spots

The codebase ships exactly one official example (the `__main__` block, in
two parts), no isolated unit tests. `balanced-tree-partitioning-demo`
exercises Lemma 2.1's three functions together on one fixed 20-vertex
tree but does not isolate them; `db-gst-t-rounding-demo` exercises
`DBGSTSolver.solve` end to end on one fixed 30-node instance but does not
isolate the LP construction, the power-of-2 rounding, the rescaling, or
the recursive rounding steps individually -- a fault confined to a code
path this particular instance does not stress (e.g. a rescaling bug that
only bites when more vertices tie at the same level) could in principle
survive both checks. No upstream repository or reference output exists to
cross-check the pin against (self-authored code); the pinned build's own
native-run output is the anchor. Following the same pattern accepted for
`consistent-kmedian`, this thin-coverage gap is flagged here for the human
to decide whether to close via future `custom` checks, rather than
resolved unilaterally.

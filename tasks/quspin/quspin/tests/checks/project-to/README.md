# project-to

Upstream test: `code/quspin/test/test_project_to.py`. Policy: `pointwise`.

## The test

Builds a `spin_basis_general` L=8 chain (`SAB_L`) in 5 representative
symmetry sectors (full, half-filling, spin-inversion, spin-inversion+parity,
spin-inversion+parity+translation), and grades `basis.project_from(v)` and
`basis.project_to(v_full)` on fixed seeded normalised random vectors: the
projected amplitude magnitudes ("projected amplitudes as |amplitude|").
Runs in a few seconds on one core.

## The two initial conditions

There is no Hamiltonian coupling here to perturb, so this check deviates
from the leaf's usual convention: the first component of both seeded input
vectors is shifted by `v0_perturbation` before renormalising and
projecting, which moves every graded amplitude through the (generically
dense) projector map. This shift changes the input vector's *direction*,
not an overall scale (renormalisation redistributes the shift across every
component's relative weight). An earlier revision used `1e-13` absolute,
which measured a spread only about one order of magnitude above the
confirmed-zero repeat-run floor; `v0_perturbation` is now `1e-11` absolute,
landing the spread three orders of magnitude above the floor.

## The pass policy

Pointwise comparison of every entry of the 5 sectors' `project_from_abs_sorted`
and `project_to_abs_sorted` arrays against `1e-8 + 1e-8*|reference|`.

## Evidence

Runtime, spread and reward are written by `sab.py task selfcheck`.

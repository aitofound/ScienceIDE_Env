# Dust/growth module coverage ledger

Authority: Phantom source commit `e53ea16758d2a261680506852a528f21270dca1c`.
All rows below are active in `tests/checks.json`; no staged or zero-weight row exists.
“Upstream test” means the listed `src/tests` routine itself makes the numerical
decision with its own threshold. “Production” means an untouched registered
setup executes a bounded solver interval and is guarded by mode/step/dump/finiteness
invariants without adding a floating-point tolerance.

| Owned boundary / mode | Executed path | Active checks |
|---|---|---|
| `dust.f90`: `init_drag`, Epstein/Stokes selection | both `idrag=1,2`; zero-slip regime | `drag-initialisation`, `epstein-zero-slip-regime` |
| `dust.f90`: stopping time, molecular viscosity, regime transition | 11 densities and 1001 sizes; nonlinear Epstein comparison | `epstein-stokes-transition` |
| `force.F90` two-fluid explicit back-reaction | gas/dust accelerations and energy exchange | `drag-conservation-explicit` |
| implicit two-fluid drag update | implicit momentum/energy exchange | `drag-conservation-implicit` |
| one-fluid dust fraction / terminal-velocity diffusion | fraction sum and analytic diffusion profile | `dustydiffuse-one-fluid` |
| `growth.f90`: initialisation/options | `ifrag=0..2`, `isnow=0..2` | `growth-initialisation-matrix` |
| growth kernel, two-fluid | Stokes-number and size evolution | `farmingbox-growth-two-fluid` |
| growth kernel, one-fluid | Stokes/size plus gas sound-speed/density interpolation | `farmingbox-growth-one-fluid` |
| fragmentation kernel, two-fluid | fragmentation size/Stokes evolution | `farmingbox-fragmentation-two-fluid` |
| fragmentation kernel, one-fluid | fragmentation plus mixture interpolation | `farmingbox-fragmentation-one-fluid` |
| settling through drag plus differential external force | registered `SETUP=dustsettle`, two-fluid dust, short evolved interval | `dustsettle-short-two-fluid` |
| Bowen dust opacity and sink radiation coupling in `dust_formation.f90`/`ptmass_radiation.f90` | official wind analytic mass/ejection invariants | `bowen-dust-radiative-wind` |

## Transitive, not separately owned

SPH neighbour search, generic density summation, generic leapfrog scheduling,
EOS, sink integration, setup prompting, wind injection geometry, external gravity,
and dump serialization are retained as dependencies. Their non-dust algorithms
belong to other module leaves. The wind checks are admitted only where their
acceptance evidence is dust opacity or nucleation specific; transonic wind alone
is not a row.

## Limits of the current policy

The production row proves runtime reachability and basic physical validity but do
not introduce calibrated field-by-field accelerator equivalence bands. This is
intentional: no human-approved numerical tolerance exists for that short
window. The upstream analytic/unit rows provide the quantitative acceptance
policy for the expensive dust/growth kernels. A later human calibration may add
production-state comparisons but must not silently alter these rubrics.

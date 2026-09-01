# phantom-dust-growth coverage ledger

Authority: human-approved manifest `Telegram#3222`; active denominator is 12.
Every row below reuses an official Phantom test file pinned under this leaf's
`code/` tree. No custom check is proposed, and the excluded task-authored
settling evolution remains outside this active ledger.

| In-scope path | Official exercised path | Check(s) |
|---|---|---|
| `src/main/dust_formation.f90` | Bowen dust opacity and radiative acceleration path selected by `test_wind.f90` | `bowen-dust-radiative-wind` |
| `src/tests/test_dust.f90` | Drag-law initialisation; zero-slip Epstein regime; Epstein/Stokes transition and nonlinear Epstein law; explicit and implicit drag conservation; one-fluid DUSTYDIFFUSE | `drag-initialisation`, `epstein-zero-slip-regime`, `epstein-stokes-transition`, `drag-conservation-explicit`, `drag-conservation-implicit`, `dustydiffuse-one-fluid` |
| `src/tests/test_growth.f90` | Growth initialisation matrix; FARMINGBOX growth and fragmentation in one-fluid and two-fluid modes | `growth-initialisation-matrix`, `farmingbox-growth-one-fluid`, `farmingbox-growth-two-fluid`, `farmingbox-fragmentation-one-fluid` *(acceleration)*, `farmingbox-fragmentation-two-fluid` |
| `src/tests/test_wind.f90` | Official Bowen dust + radiative acceleration scenario and injected-mass invariants | `bowen-dust-radiative-wind` |

The acceleration label is reserved for `farmingbox-fragmentation-one-fluid`,
whose official 20,000-cell analytic FARMINGBOX loop is the repeated expensive
path. The table records the intended label placement; label metadata is already
present in that check and is not changed by this curation step.

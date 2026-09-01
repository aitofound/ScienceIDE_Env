# Coverage ledger — 4 pinned upstream SR-hydro regression modules (contract v4)

Pinned source commit: `823614c90b594472747a0ac2a699e4a454f300d2`. Dispatcher: `code/athena/tst/regression/run_tests.py`. Each row is one direct check = one upstream module under `tst/regression/scripts/tests/sr/` executed end to end; the call structure was extracted by dry-running every module with stubbed `athena.*` helpers (re-derived by `tests/test_contract_guards.py` on every run) and is enforced by the verifier as an exact multiset (`tests/sr_hydro_contract.json` → `expected_calls`).

| ID | Direct check | Upstream module | Decks | configure/make | run / mpirun / restart | Native outputs compared | Upstream analyzer (oracle sanity on the reference) |
|---|---|---|---|---|---|---|---|
| SH-01 | `sr-hydro-convergence` | `sr/hydro_convergence.py` | `hydro_sr/athinput.linear_wave` | 1/1 (`-s --prob=gr_linear_wave --coord=cartesian --flux=hllc`) | 10 / 0 / 0 (wave flags 0-4 at 64 and 512 cells) | `bin/sr_hydro_wave_{flag}_{low,high}.block0.out1.*.tab`, primitive variables, `%24.16e` text | RMS/amp error of columns 1..5 must converge with `epsilon_512/epsilon_64 <= (64/512)^1.8` for every flag — `sr/hydro_convergence.py:62-106` |
| SH-02 | `sr-hydro-shocks-hllc` | `sr/hydro_shocks_hllc.py` | `hydro_sr/athinput.mb_1..mb_4` | 1/1 (`-s --prob=gr_shock_tube --coord=cartesian --flux=hllc`) | 4 / 0 / 0 (400 cells, tlim 0.4) | `bin/sr_hydro_shock{1..4}.block0.out1.*.vtk`, conservative variables, float32 | relative L1 of dens/Etot/mom[0] against `tst/regression/data/sr_hydro_shock{1..4}_hllc.vtk` within `[0.02,0.01,0.01] [0.01,0.01,0.02] [0.01,0.01,0.02] [0.5,0.01,0.02]` — `sr/hydro_shocks_hllc.py:42-63` |
| SH-03 | `sr-hydro-shocks-hlle` | `sr/hydro_shocks_hlle.py` | `hydro_sr/athinput.mb_1..mb_4` | 1/1 (`… --flux=hlle`) | 4 / 0 / 0 | as SH-02 | as SH-02 (same baselines and rows) — `sr/hydro_shocks_hlle.py:42-63` |
| SH-04 | `sr-hydro-shocks-llf` | `sr/hydro_shocks_llf.py` | `hydro_sr/athinput.mb_1..mb_4` | 1/1 (`… --flux=llf`) | 4 / 0 / 0 | as SH-02 | as SH-02 (same baselines and rows) — `sr/hydro_shocks_llf.py:42-63` |

**Totals:** 4 configure/make cycles, 22 Athena++ launches. All four builds are serial (`-s`); no MPI, HDF5 or FFTW is exercised.

## Science mode

All four rows use `reference-tolerance`: the CPU reference root must satisfy the pinned upstream `analyze()` (re-executed by the verifier in its trusted tree); the candidate's native `bin/` outputs are compared to the reference's value by value under `abs(candidate - reference) <= max(1e-12 + 1e-8*|reference|, precision floor)`. The floor is `1e-6*|reference| + 1e-12` for float32 payloads (Athena++ legacy VTK is always float32, `src/outputs/vtk.cpp`), so SH-02..SH-04 are effectively judged at `1e-12 + 1e-6*|reference|`; for SH-01 the text floor (one unit in the last printed digit of a `%24.16e` token, `10^(exponent-16)`) is far below the base rule, so `1e-12 + 1e-8*|reference|` is effective. Per-check overrides are null. The candidate's own upstream `analyze()` result is reported as information only.

## Why the upstream analyzers are oracle-sanity gates, not the verdict

The three shock analyzers compare against static upstream baseline VTK files (`sr_hydro_shock{1..4}_hllc.vtk`, produced with HLLC) with tolerances of 1-50 %, and the convergence analyzer checks an order-of-accuracy ratio; both are satisfied by any reasonable SR hydro implementation, so on their own they cannot tell a faithful port from a loosely similar one. The verifier therefore requires them only of the CPU reference and judges the candidate by closeness to that reference.

## Acceleration

`sr-hydro-convergence` (SH-01) carries the `acceleration` label: ten one-dimensional runs, five of them at 512 cells for roughly one wave crossing time each; it is the heaviest of the four modules in zone-cycles. The verifier reports per-check zone-cycle and CPU-second sums from the native termination blocks so the owner can revisit that choice.

## Other pinned upstream SR scripts (not selected; owner decision)

`code/athena/tst/regression/scripts/tests/sr/` also contains `mhd_convergence.py`, `mhd_shocks_hlld.py`, `mhd_shocks_hlle.py`, `mhd_shocks_llf.py` and `sr_mhd_linwave.py` (all `-s -b`, magnetic fields — the `athena-sr-mhd` leaf's territory), and `tst/regression/scripts/tests/gr/` contains the Minkowski-metric shock tubes `hydro_shocks_hllc.py`, `hydro_shocks_hlle.py`, `hydro_shocks_llf.py`, `hydro_shocks_hlle_no_transform.py`, `hydro_shocks_llf_no_transform.py` (`-g`, `--coord=minkowski`, same `gr_shock_tube` generator in flat spacetime) and `compile_minkowski.py`. None of these is part of this leaf's human-approved module set.

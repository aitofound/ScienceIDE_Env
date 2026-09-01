# Coverage ledger — 4 pinned upstream MHD regression modules (contract v4)

Pinned source commit: `823614c90b594472747a0ac2a699e4a454f300d2`. Dispatcher: `code/athena/tst/regression/run_tests.py`. Each row is one direct check = one upstream module executed end to end; the call structure was extracted by dry-running every module with stubbed `athena.*` helpers and is enforced by the verifier as an exact multiset (`tests/mhd_contract.json` → `expected_calls`).

| ID | Direct check | Upstream module | Decks | configure/make | run / mpirun / restart | Science re-derived by the verifier | Upstream analyzer |
|---|---|---|---|---|---|---|---|
| NM-01 | `linear-wave-3d` | `mhd/mhd_linwave.py` | `mhd/athinput.linear_wave3d` | 2/2 (`-b --prob=linear_wave --coord=cartesian --flux=hlld|roe`) | 20 / 0 / 0 | fast/Alfvén/slow/entropy RMS error bounds (4.5e-8, 4.0e-8, 5.0e-8, 2.75e-8), convergence ratios <= 0.4 between 32 and 64 cells, max/RMS ratio bounds and L/R fast-wave error equality per flux from bin/linearwave-errors.dat | `tst/regression/scripts/tests/mhd/mhd_linwave.py:97-164` |
| NM-02 | `cpaw-2d` | `mhd/cpaw.py` | `mhd/athinput.cpaw2d` | 1/1 (`-b --prob=cpaw --eos=isothermal --flux=hlld`) | 3 / 0 / 0 | L1 error <= 2.0e-4 at 256 cells, convergence ratio <= 0.3 between 128 and 256 cells, L/R-going wave errors equal within 2.0e-6 from bin/cpaw-errors.dat | `tst/regression/scripts/tests/mhd/cpaw.py:50-75` |
| NM-03 | `rj2a-shock` | `mhd/rj2a_shock.py` | `mhd/athinput.rj2a` | 2/2 (`-b --prob=shock_tube --coord=cartesian --flux=hlld|roe`) | 12 / 0 / 0 | equal cycle counts across x1/x2/x3, convergence ratio <= 0.6^log2(nx/256) and error <= 0.01 at 512 cells per direction and flux from bin/shock-errors.dat | `tst/regression/scripts/tests/mhd/rj2a_shock.py:76-111` |
| NM-04 | `carbuncle-robustness` | `mhd/mhd_carbuncle.py` | `hydro/athinput.quirk` | 5/5 (`-b --prob=quirk --coord=cartesian --flux=hlle|roe|llf|lhlld|hlld`) | 5 / 0 / 0 | even/odd post-shock P/rho^gamma difference <= 0.05 for hlle, llf and lhlld (hlld and roe are exempt upstream) from bin/carbuncle-diff.dat | `tst/regression/scripts/tests/mhd/mhd_carbuncle.py:50-68` |

**Totals:** 10 configure/make cycles, 40 Athena++ launches. No module uses `mpirun` or `restart`, MPI, FFTW or HDF5.

## Science mode

All four rows are `reference-tolerance`: the CPU reference root must satisfy the pinned upstream `analyze()` (re-executed by the verifier in its trusted tree); the candidate's native `bin/` outputs are compared to the reference's value by value under `abs(candidate - reference) <= max(1e-12 + 1e-8*abs(reference), precision floor)` — floor `1e-6*|reference| + 1e-12` for float32 payloads and one unit in the last printed digit for text tokens (suite rule; per-check override null). The candidate's own upstream `analyze()` result is reported as information only.

## Native outputs compared per check

| Direct check | Text tables (print-grain floor) | float32 binary (1e-6 floor) |
|---|---|---|
| `linear-wave-3d` | `linearwave-errors.dat` (20 rows, `%e`), `LinWave.hst` (`%e`) | — (`output2/dt=-1` disables the VTK dump) |
| `cpaw-2d` | `cpaw-errors.dat` (3 rows), `cpaw.hst` | — (`output2/dt=-1`) |
| `rj2a-shock` | `shock-errors.dat` (12 rows), `RJ2a.hst`, `RJ2a.block0.out1.NNNNN.tab` (`%12.5e`, last run's dumps) | — |
| `carbuncle-robustness` | `carbuncle-diff.dat` (5 rows) | `Quirk.block*.out1.NNNNN.vtk` (last run's dumps) |

Each module overwrites the same `problem_id` outputs across its runs, so the packaged `bin/` holds the final run's dumps plus the cumulative error tables that the analyzers read.

## Known exact-equality rules in the upstream analyzers (informational for the candidate)

- `mhd_linwave.py:159` — `data[8][4] != data[9][4]`: the L- and R-going fast-wave RMS errors must be bit-identical.
- `rj2a_shock.py:92` — `cycles[0] != cycles[1]`: cycle counts must be identical across shock directions.

Both are enforced on the CPU reference (oracle sanity) and reported as a warning only when the candidate misses them.

## Coverage boundary (owner decision)

These four scripts are the entire `tst/regression/scripts/tests/mhd/` directory at the pinned commit, so the leaf covers what upstream registers as "mhd" — but that is thin for the whole Newtonian ideal-MHD path. Pinned material the owner could add, none of it included here:

- Newtonian scripts in other groups that configure with `-b`: `amr/amr_linwave.py`, `diffusion/linear_wave3d.py`, `diffusion/resistive_diffusion.py`, `eos/eos_mhd.py`, `hybrid/hybrid_linwave.py`, `mpi/mpi_linwave.py`, `omp/omp_linwave.py`, `shearingbox/mhd_shwave.py`, `shearingbox/mri2d.py`, and the infrastructure rows already carried by the hydro leaf (`outputs/all_outputs.py`, `pgen/hdf5_reader_*.py`, `pgen/pgen_compile.py`).
- MHD problem generators with decks but no upstream regression script: `field_loop`, `field_loop_poles`, `orszag_tang`, `rotor`, and the `-b` variants of `blast`, `kh` and `rt` (a check for these would need a task-local analyzer, which v4 deliberately avoids).
- Relativistic MHD (`sr/mhd_*.py`, `sr/sr_mhd_linwave.py`, `gr/mhd_shocks_*.py`) is a different production path and out of scope for a "Newtonian" leaf.

Adding any of them is an owner decision; this leaf keeps the human-approved four.

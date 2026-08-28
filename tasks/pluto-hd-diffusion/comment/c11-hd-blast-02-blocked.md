# C11 HD/Blast configuration 02 preprocessing resolution

The former C11 Docker attempt reached the PLUTO solver but stopped because the
official `InitDomain()` implementation consumes the preceding campaign's
external `grid0.out` and `rho0.dbl` products. That missing-input condition is
resolved in this task without changing `code/pluto`:

- `tests/checks/c11-hd-blast-02/build/deck.py` deterministically generates a
  200 x 200 x 1 Cartesian `grid0.out` and a positive little-endian FP64
  `rho0.dbl` during Docker image preprocessing.
- `tests/checks/c11-hd-blast-02/run.sh` copies those products into the isolated
  `/app/results` working directory before PLUTO starts.
- The generated-input hashes and dimensions are recorded in
  `deck_manifest.json`; the native `grid.out`, `dbl.out`, and
  `data.%04d.dbl` products remain solver-owned outputs.

The repaired retained Docker run completed with `solver_exit_status=0`; its
native output contract parsed 3 frames with 40,000 cells and variables
`rho,vx1,vx2,vx3`. The complete C01-C35 continuation now treats C11 as an ordinary acceptance
row; it does not use a non-numeric exception for this input path.

This filename is retained as historical evidence of the original issue. It no
longer describes the row's status or acceptance behavior.

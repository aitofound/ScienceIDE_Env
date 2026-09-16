# nurbs-poisson-no-integration-by-parts

Poisson on a curved 2D pipe with the weak form assembled without integration by parts, so the second derivatives of the basis are used directly, at two refinement levels.

## What this check runs

`run.sh` builds the pinned MFEM source with `make serial MFEM_USE_METIS=NO` at MFEM's own
`-O3 -std=c++17`, then runs 2 registered configuration(s) of
`code/mfem/miniapps/nurbs/nurbs_ex1.cpp`, each in its own staged directory:

- `nurbs_ex1 -m ../../data/pipe-nurbs-2d.mesh -o 2 -no-ibp -r 0`
- `nurbs_ex1 -m ../../data/pipe-nurbs-2d.mesh -o 2 -no-ibp -r 2`

Meshes named on the command line are passed by absolute path. A miniapp that is given no
mesh opens a compiled-in default two directory levels above its own location, so each run
still executes two levels below a per-run root with the mesh tree linked in at that height and
the binary's own directory linked beside it.

## Initial conditions

`ic/nominal/files/` carries the mesh set this check solves on, copied from the pinned source;
`ic/nominal/manifest.txt` lists them and `run.sh` overlays them onto its source copy before
building. `ic/variant/` is the same set with one control-point coordinate moved by 1e-8
relative to the mesh's coordinate scale.

## Output files this check produces

- `r01-pipe-nurbs-2d-o2-no-ibp-r0__sol.gf`
- `r02-pipe-nurbs-2d-o2-no-ibp-r2__sol.gf`

`*.gf` and `*.sol` are MFEM grid functions in their text form: a short header naming the space, then one value per line. The grader reads only the lines whose every whitespace-separated token parses as a number.

`refined.mesh` and the VisIt collection each run also writes are **not** graded: the VisIt
stream is written at precision 6 rather than 8, and element numbering after refinement is a
legitimate implementation choice rather than physics. A genuinely different discrete space is
still caught, because the solution stream then has a different length.

## Knobs

`run.sh --help` lists them. Defaults are the graded values.

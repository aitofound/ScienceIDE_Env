# nurbs-solenoidal-fields

Construction of a pointwise divergence-free velocity field in a compatible spline space, on a curved 2D patch and in 3D.

## What this check runs

`run.sh` builds the pinned MFEM source with `make serial MFEM_USE_METIS=NO` at MFEM's own
`-O3 -std=c++17`, then runs 2 registered configuration(s) of
`code/mfem/miniapps/nurbs/nurbs_solenoidal.cpp`, each in its own staged directory:

- `nurbs_solenoidal -m ../../data/pipe-nurbs-2d.mesh -r 1 -o 2`
- `nurbs_solenoidal -m ../../data/cube-nurbs.mesh -r 1 -o 2`

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

- `r01-pipe-nurbs-2d-r1-o2__sol_u.gf`
- `r01-pipe-nurbs-2d-r1-o2__sol_p.gf`
- `r01-pipe-nurbs-2d-r1-o2__errors.txt`
- `r01-pipe-nurbs-2d-r1-o2__verdicts.txt`
- `r02-cube-nurbs-r1-o2__sol_u.gf`
- `r02-cube-nurbs-r1-o2__sol_p.gf`
- `r02-cube-nurbs-r1-o2__errors.txt`
- `r02-cube-nurbs-r1-o2__verdicts.txt`

`*__errors.txt` holds one number per line: the value printed after the final `=` of each of this target's error lines, in the order listed above.

`*__verdicts.txt` holds one integer per line, `1` or `0`: whether a residual whose exact value is zero came in at or below the threshold `run.sh` states for it. Such a residual is graded by this verdict and never by its value, because a different build simply replaces one round-off remainder with another of the same order.

`*.gf` and `*.sol` are MFEM grid functions in their text form: a short header naming the space, then one value per line. The grader reads only the lines whose every whitespace-separated token parses as a number.

`refined.mesh` and the VisIt collection each run also writes are **not** graded: the VisIt
stream is written at precision 6 rather than 8, and element numbering after refinement is a
legitimate implementation choice rather than physics. A genuinely different discrete space is
still caught, because the solution stream then has a different length.

## Knobs

`run.sh --help` lists them. Defaults are the graded values.

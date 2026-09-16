# nurbs-two-patch-interface-matching

Two patches joined along an edge in 2D and across a face in 3D, each in three variants: the interface declared in the mesh file, the second patch rotated so the parametric directions disagree, and the interface detected automatically. Refinement comes from an explicit refinement file rather than uniformly.

## What this check runs

`run.sh` builds the pinned MFEM source with `make serial MFEM_USE_METIS=NO` at MFEM's own
`-O3 -std=c++17`, then runs 6 registered configuration(s) of
`code/mfem/miniapps/nurbs/nurbs_ex1.cpp`, each in its own staged directory:

- `nurbs_ex1 -m meshes/two-squares-nurbs.mesh -o 1 -rf meshes/two-squares.ref`
- `nurbs_ex1 -m meshes/two-squares-nurbs-rot.mesh -o 1 -rf meshes/two-squares.ref`
- `nurbs_ex1 -m meshes/two-squares-nurbs-autoedge.mesh -o 1 -rf meshes/two-squares.ref`
- `nurbs_ex1 -m meshes/two-cubes-nurbs.mesh -o 1 -r 3 -rf meshes/two-cubes.ref`
- `nurbs_ex1 -m meshes/two-cubes-nurbs-rot.mesh -o 1 -r 3 -rf meshes/two-cubes.ref`
- `nurbs_ex1 -m meshes/two-cubes-nurbs-autoedge.mesh -o 1 -r 3 -rf meshes/two-cubes.ref`

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

- `r01-two-squares-nurbs-o1__sol.gf`
- `r02-two-squares-nurbs-rot-o1__sol.gf`
- `r03-two-squares-nurbs-autoedge-o1__sol.gf`
- `r04-two-cubes-nurbs-o1-r3__sol.gf`
- `r05-two-cubes-nurbs-rot-o1-r3__sol.gf`
- `r06-two-cubes-nurbs-autoedge-o1-r3__sol.gf`

`*.gf` and `*.sol` are MFEM grid functions in their text form: a short header naming the space, then one value per line. The grader reads only the lines whose every whitespace-separated token parses as a number.

`refined.mesh` and the VisIt collection each run also writes are **not** graded: the VisIt
stream is written at precision 6 rather than 8, and element numbering after refinement is a
legitimate implementation choice rather than physics. A genuinely different discrete space is
still caught, because the solution stream then has a different length.

## Knobs

`run.sh --help` lists them. Defaults are the graded values.

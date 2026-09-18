# screened-poisson-sphere

Screened Poisson on an isoparametric triangulation of the unit sphere at order 2 with four refinements, and the same problem on a quadrilateral sphere with the refined nodes snapped back onto the exact sphere. Both print the L2 error against the exact solution.

## What this check runs

`run.sh` builds the pinned MFEM source with `make serial MFEM_USE_METIS=NO` at MFEM's own
`-O3 -std=c++17` (parallel jobs bounded by the `SAB_CPUS` knob, default the declared 4), then
runs 2 registered configuration(s) of `code/mfem/examples/ex7.cpp`, each in its own staged
directory:

- `ex7 -e 0 -o 2 -r 4`
- `ex7 -e 1 -o 2 -r 4 -snap`

Meshes named on the command line are passed by absolute path. An example given no mesh opens
a compiled-in default one or two directory levels above its own location, so each run
executes below a per-run root with the mesh tree linked in at that height.

## Initial conditions

`ic/nominal/files/` carries the mesh set this check solves on, copied from the pinned source;
`ic/nominal/manifest.txt` lists them and `run.sh` overlays them onto its source copy after
building. `ic/variant/` is the same set with one vertex coordinate moved by 1e-8 relative to
the mesh's coordinate scale -- for this check there is no mesh file to perturb and the variant is declared identical in `rubric.json`.

## Output files this check produces

- `r01-e0-o2-r4__sol.gf`
- `r01-e0-o2-r4__errors.txt`
- `r02-e1-o2-r4-snap__sol.gf`
- `r02-e1-o2-r4-snap__errors.txt`

`*__errors.txt` holds one number per line: the last token of each of this example's printed error lines, in the order listed above.

`*.gf` is an MFEM grid function in its text form: a short header naming the space, then one value per line. The grader reads only the lines whose every whitespace-separated token parses as a number.

The mesh each run also writes after refining it is **not** graded: element numbering after
refinement is a legitimate implementation choice rather than physics. A genuinely different
discrete space is still caught, because the solution stream then has a different length.

## Knobs

`run.sh --help` lists them. Defaults are the graded values.

# nurbs-mesh-topology-report

A report of the patch topology, knot vectors, degrees and control-point counts of a NURBS mesh.

## What this check runs

`run.sh` builds the pinned MFEM source with `make serial MFEM_USE_METIS=NO` at MFEM's own
`-O3 -std=c++17`, then runs 1 registered configuration(s) of
`code/mfem/miniapps/nurbs/nurbs_mesh_info.cpp`, each in its own staged directory:

- `nurbs_mesh_info `

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

- `r01-default__k0_cheby.dat`
- `r01-default__k0_n0.dat`
- `r01-default__k0_n1.dat`
- `r01-default__k1_cheby.dat`
- `r01-default__k1_n0.dat`
- `r01-default__k1_n1.dat`

`refined.mesh` and the VisIt collection each run also writes are **not** graded: the VisIt
stream is written at precision 6 rather than 8, and element numbering after refinement is a
legitimate implementation choice rather than physics. A genuinely different discrete space is
still caught, because the solution stream then has a different length.

## Knobs

`run.sh --help` lists them. Defaults are the graded values.

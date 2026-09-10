# cimi-plasmasphere

Official source target: `code/swmf/IM/CIMI/Makefile:PLASMASPHERE`.
Policy: `pointwise` (proposed; calibration pending).

## The test

`run.sh` copies the pinned SWMF tree, performs the official top-level install and the CIMI `-EarthHO -GridDefault` configuration, and invokes the exact source-backed `PLASMASPHERE_compile` target. Because the upstream `PLASMASPHERE` wrapper is compile+rundir+execute, the repaired driver then calls only the safe source `rundir` target with a fresh absent task-private `RUNDIR`, symlinks the current `BINDIR/unit_test_plasmasphere.exe` into it, and executes there. Compiled objects/libraries and that executable may be reused from an immutable compiled-only cache; no run directory, writer output, or symlink into a prior work tree is cached. The source default is retained: a 209 by 192 plasmasphere grid, a 30-second update, a one-day physical window, and the Kp ramp and potential construction written in the unit test. No source patch, shortened physical window, invented parameter, or custom executable is used.

The collector requires the real fresh-run writer output `RUNDIR/IM/plots/Plas_eq.outs` and copies it to the fixed graded name `Plas_eq.outs`. This is the output written by `ModPlasmasphere.save_plot_plasmasphere` (`IM/CIMI/src/ModPlasmasphere.f90`, `NamePlotEq` and `save_plot_file`), and it carries the physical density, flux-tube volume and convection potential arrays plus their grid coordinates. A missing or empty file is an error; no silent skip is possible. `SAB_BUILD_SECONDS` is emitted before execution and excludes the simulation.

The only runtime knob is `SAB_MAKE_JOBS`, which changes build parallelism only. The `SAB_COLLECTOR_FIXTURE` path exists solely for cheap writer-shaped fixture tests and is not a graded execution or science result. The task's suite budget excludes the build, and no runtime measurement or PASS result is claimed in this implementation-only step.

## The two initial conditions

The upstream unit test has no input file, command-line parameter, random seed, or exposed runtime setting. `ic/nominal/` and `ic/variant/` are therefore intentionally empty and the driver executes the same source-defined initial condition for both. This is an explicit identical-variant record, not calibration evidence and not a claim that a future implementation may ignore perturbations. A meaningful perturbed input requires an upstream source input or an explicitly approved source change, neither of which is authorized here.

## The pass policy

The proposed policy compares every numeric value in the pinned `Plas_eq.outs` stream using the stock SWMF stream reader: physical coordinates, density, flux-tube volume, convection potential and each saved frame are parsed from the writer-shaped output, while the plot-step bookkeeping field is excluded as non-physical by the validator. The provisional bound is `1e-10 + 1e-3*|reference|`; it is not approved and has no measured floor or spread. A wrong refill/loss update, convection potential, plasmasphere advance, interpolation, or writer shape should change the production arrays, but whether pointwise comparison and this bound remain achievable is for fresh target-host calibration. The source has no baseline/check recipe, so this check does not pretend that a historical or generated baseline is available.

## Evidence

Source evidence is the pinned `IM/CIMI/Makefile` target and the two writer locations cited above. Baseline output, build/run seconds, nominal-versus-variant spread, and self-validation are all `unknown` pending the later consented calibration run. The implementation validates the collector and validator against real writer-shaped fixture names only; those fixtures are not solver results.

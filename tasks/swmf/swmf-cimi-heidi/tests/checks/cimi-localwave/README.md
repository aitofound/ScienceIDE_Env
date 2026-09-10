# cimi-localwave

Official source target: `code/swmf/IM/CIMI/Makefile:LOCALWAVE`.
Policy: `pointwise` (proposed; calibration pending).

## The test

`run.sh` copies the pinned SWMF tree, performs the official top-level install and the CIMI `-EarthHO -GridDefault` configuration, and invokes the exact source-backed `LOCALWAVE_compile` target. Because the upstream `LOCALWAVE` wrapper is compile+rundir+execute, the repaired driver then calls only the safe source `rundir` target with a fresh absent task-private `RUNDIR`, symlinks the current `BINDIR/unit_test_localwave.exe` into it, and executes there. Compiled objects/libraries and that executable may be reused from an immutable compiled-only cache; no run directory, writer output, or symlink into a prior work tree is cached. The source-defined defaults are retained: its hard-coded energy and pitch-angle grids, analytic synthetic PSD, 149 nT field, 16e6 m^-3 density, one-second time step, and 60-step window. No source patch, invented deck, or shortened physical window is used.

The collector requires all five files written by the pinned `write_debug_output` routine and copies them without transformation: `localwaveP_ISD.psd`, `localwaveP_ISDinterp.psd`, `localwave_dfdppar.psd`, `localwave_dfdpperp.psd`, and `localwave_resonances.dat`, all under the fresh `RUNDIR/IM/plots`. The files contain the analytic/input PSD, its Cartesian interpolation, parallel and perpendicular derivatives, and the resonant-frequency/wave-power diagnostics. Every expected file must exist and be non-empty; missing output is a hard failure. `SAB_BUILD_SECONDS` is emitted before execution and excludes the simulation.

The only runtime knob is `SAB_MAKE_JOBS`, which changes build parallelism only. The `SAB_COLLECTOR_FIXTURE` path exists solely for cheap writer-shaped fixture tests and is not a graded execution or science result. The task's suite budget excludes the build, and no runtime measurement or PASS result is claimed in this implementation-only step.

## The two initial conditions

The upstream unit test has no input file, command-line parameter, or exposed random seed. `ic/nominal/` and `ic/variant/` are therefore intentionally empty and the driver executes the same source-defined initial condition for both. This is an explicit identical-variant record, not calibration evidence and not a claim that a future implementation may ignore perturbations. A meaningful perturbed input requires an upstream source input or an explicitly approved source change, neither of which is authorized here.

## The pass policy

The proposed policy compares every numeric value in the five pinned writer streams with the stock SWMF stream reader. It grades the physical phase-space density and interpolation, both PSD gradients consumed by the wave model, and the resonance frequency/wave-power quantities; it does not substitute a constant or a storage-only sentinel. The provisional bound is `1e-10 + 1e-3*|reference|`; it is not approved and has no measured floor or spread. A wrong interpolation, derivative, resonance calculation, or wave-power update should alter these production values, but fresh target-host calibration must determine whether pointwise comparison and this bound remain achievable. The source target has no baseline/check recipe, so this check does not pretend that a historical or generated baseline is available.

## Evidence

Source evidence is the pinned `IM/CIMI/Makefile:LOCALWAVE` target and every writer path in `IM/CIMI/src/unit_test_localwave.f90` (`write_debug_output`). Baseline output, build/run seconds, nominal-versus-variant spread, and self-validation are all `unknown` pending the later consented calibration run. The implementation validates the collector and validator against real writer-shaped fixture names only; those fixtures are not solver results.

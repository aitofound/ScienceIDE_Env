# Changelog

## Developer Notes

When incrementing the version number to X.Y.Z, please do the following
* create a new subsection here (below **NEXT**) with the title vX.Y.Z (YYYY-MM-DD)
* update the version number in `configure.ac` and in `src/global.h`
* commit with the message "Increment version to X.Y.Z"
* create a tag named "vX.Y.Z" with the message "version X.Y.Z"
* push and follow tags

## NEXT

## v0.15.0 (2025-09-22)

- Update `parkerB` to match `mhdB` in the ideal-shock case
- Allow `rigidityPower` to be 0.0 (previously restricted to >= 1e-33)
- Update example configuration files
- Fix memory leaks

## v0.14.0 (2025-04-05)

- Allow boolean-like input parameters to take the value true or false
- Fix computations of MHD `Bphi` and `Vr` to account for transformation between
  shock frame and inertial frame
- Add `idealShockGradient` parameter
- Deprecate `idealShockScale` parameter
- Remove the following unused parameters. These parameters did not appear
  anywhere in the code base outside of configuration.c and configuration.h
  - `dsh_hel_min`
  - `useStochastic`
  - `fluxLimiter`
  - `gammaEhigh`
  - `gammaElow`
  - `FailModeDump`
  - `subTimeCouple`
  - `epEquilibriumCalcDuration`
  - `mhdInitMonteCarlo`
  - `fieldAligned`
  - `shockSolver`
  - `shockDetectPercent`
  - `minInjectionEnergy`
  - `shockInjectionFactor`
  - `outputRestart`
  - `dumpOnAbort`
  - `saveRestartFile`

## v0.13.0 (2025-04-04)

- Deprecate `useShellDiffusion` user parameter
- Automatically set internal `useShellDiffusion` based on value of `kperxkpar`
- Add `boundaryFunctE0` parameter
- Deprecate `boundaryFunctEr` parameter

## v0.12.0 (2025-03-13)

- Add mean free path to observer output arrays
- Allow the special case of `kperxkpar == 0.0` to turn off perpendicular diffusion
- Allow the special case of `interpDistance == 0.0` to remove the distance
  threshold on point-observer interpolation

## v0.11.0 (2025-02-17)

- Add the option to run with a parameterized magnetic switchback
- Write point observer flux to file when requested
- Add `outputFlux` parameter
- Deprecate `streamFluxOutput`

## v0.10.1 (2025-02-06)

- Fix bug in radially-scaled mean free path
- Add `mfpPower` parameter
- Deprecate `mfpRadialPower`

## v0.10.0 (2025-02-03)

- Add `mfpInverseB` parameter
- Scale mean free path inversely to the magnetic-field magnitude when `mfpInverseB=1`
- Add functions to inform user of replaced and deprecated parameters
- Define `interpWeight` and `interpDistance` (unit=au; default=1.0) parameters
- Refactor point-observer distribution interpolation

## v0.9.0 (2025-01-14)

- Extract `updateNodeMhd` from `updateMhd` to allow reuse of MHD functions
- Modify point-observer MHD output
- Fix bug in point-observer distribution interpolation
- Update radial condition for ideal shock node
- Write runtime parameter values to a dedicated file

## v0.8.0 (2024-11-07)

- Add species-specific `abundance` parameter
- Deprecate `boundaryFunctXi` parameter

## v0.7.1 (2024-10-15)

- Fix multi-species functionality

## v0.7.0 (2024-07-09)

- Output correct point-observer MHD values when not coupled to external MHD

## v0.6.1 (2024-06-07)

- Fix bug in `Makefile.am` for `make dist`: change `install.sh` to `setup.sh`
- Fix bug in `setup.sh`: check for git repo before creating default alias

## v0.6.0 (2024-06-03)

- Rename `install.sh` to `setup.sh`
- Change default file-name prefix for stream and point observers
- Implement `streamLegacyPrefix` and `pointLegacyPrefix` options
- Use the current git branch as the default alias in `setup.sh`
- Fix bug for setting point-observer angular positions in degrees
- Change `numMuSteps` from 20 to 11

## v0.5.0 (2024-05-30)

- Add command-line option to print version number and exit
- Replace `obsUseDegrees` and `idealShockUseDegrees` with single `useDegrees` parameter
- Add `DEG2RAD` and `RAD2DEG` constants
- Fix bug in `make dist`: add missing `tools.sh`
- Remove redundant parameter descriptions in `install.sh` help text
- Fix bug in default `numSpecies`, `mass`, and `charge`
- Fix error in how `mhdB` and `parkerB` compute ideal-shock magnetic field

## v0.4.0 (2024-01-25)

- Create `install.sh` to replace `setup.sh` with some features moved from `build.sh`
- Change default value of `boundaryFunctBeta` from 1.7 to 2.0
- Add reference radius and reference energy to runtime options
- Allow user to pass ideal shock angles (i.e., theta, phi, and width) in degrees
- Allow user to pass observer angles (i.e., theta and phi) in degrees
- Change value of `VERSION` to be consistent with this and configure files
- Refactor parameter-related constants
- Improve config-file read and echo
- Always output the initial time step
- Accept different widths for the ideal shock
- Define a merged shock-scale parameter
- Fix redundant installation of external dependencies

## v0.3.0 (2023-12-18)

- Implement major algorithmic updates to adiabatic change and focusing developed by PSI
- Remove species dependence of "grid" arrays for energy, velocity, and momentum
- Set the default value of `rScale` to the value of 1 solar radius in au (i.e.,
  `RSAU` as defined in `global.h`).
- Set the background (a.k.a "seed") spectrum minimum value to `DBL_MIN`
- Implement point-observer output
- Add initial components for model-agnostic MHD coupling
- Refactor and add features to build-process tools

## v0.2.6 (2023-08-03)

- Add `--download-ext-deps` to `setup.sh` CLI.

## v0.2.5 (2023-04-11)

- Redefined options for `setup.sh`.

## v0.2.4 (2023-01-06)

- Allow `idealShockFalloff` to be 0 and change default value to 0.

## v0.2.3 (2022-12-20)

- Expanded installation instructions in README.
- Added `--with-ext-deps=DIR` to configure options.
- Moved `--with`-style configure options to `setup.sh`.
- Copy `config.log` during `make install`.

## v0.2.2 (2022-11-11)

- Revert to multiplying velocity by the ideal shock factor because EPREM nodes
  are not in the frame co-moving with the shock.

## v0.2.1 (2022-11-11)

- Avoid running `autoreconf` as part of setup.sh if possible.

## v0.2.0 (2022-11-10)

- Exponentially relax shocked quantities to unshocked values downstream of shock.
- Allow user to control rate of relaxation with `idealShockFalloff`.

## v0.1.1 (2022-11-09)

- Fix velocity scaling for ideal shock in `flow.c`.

## v0.1.0 (2022-11-02)

- Initial release of uncoupled EPREM

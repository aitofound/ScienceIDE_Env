# FLEKS — FLexible Exascale Kinetic Simulator

## Overview

FLEKS is a Particle-In-Cell (PIC) and particle tracker code built on top of the
[AMReX](https://amrex-codes.github.io/) framework. It serves as the **PC**
(Particle-in-Cell) and **PT** (Particle Tracker) components within the
[SWMF](https://github.com/SWMFsoftware) (Space Weather Modeling Framework),
coupling with BATS-R-US (the MHD solver) via the MHD-AEPIC algorithm. It can
also run as a standalone AMReX/MPI executable when the SWMF coupler is not
needed.

**Primary use cases:**
- Implicit Particle-In-Cell simulations (semi-implicit θ-scheme with GMRES
  field solver)
- Hybrid Particle-In-Cell simulations (kinetic ions + massless fluid
  electrons via a generalized Ohm's law)
- Test particle tracking in electromagnetic fields
- Coupled MHD-PIC simulations within SWMF (MHD-AEPIC)
- Outer-heliosphere pickup-ion simulations (OH-PT coupling)
- Solar Energetic Particle (SEP) transport

**Language:** C++ (C++14/17), with Fortran 90 interface wrappers for SWMF
coupling.

**License:** Apache 2.0

---

## Repository Layout

```
FLEKS/
├── AGENT.md              ← You are here
├── include/              ← All header files (.h)
├── src/                  ← All implementation files (.cpp) and src Makefile
├── srcInterface/         ← SWMF coupling layer (Fortran wrappers + C++ interface)
├── Doc/                  ← User manual, algorithms, and coding standards
├── tools/                ← Post-processing and conversion scripts (Python/bash)
├── tests/                ← Standalone fixtures and future unit/regression tests
├── bin/                  ← Built executables (created by build)
├── .agent/skills/        ← Agent skills
├── .agent/workflows/     ← Agent workflows
├── Config.pl             ← Perl configuration script (AMReX, test particles, etc.)
├── Makefile              ← Top-level Makefile
├── Makefile.def.FLEKS    ← Default FLEKS Makefile definitions
├── PARAM.XML             ← Parameter command documentation (XML, ~1500 lines)
├── .clang-format         ← Clang-format configuration (Mozilla-based, 80-col)
└── .gitignore
```

## Agent Quickstart

Use this workflow for most code changes:
1. Read this file and the nearest subdirectory `AGENT.md` first.
2. If a header changes, inspect matching implementation files and rebuild `LIB`.
3. Run `make LIB -j8` for SWMF component validation; run `make EXE -j8`
   when standalone executable behavior is affected.
4. Run `make compile_commands` after include-path or build-flag changes.

### Key directories

| Directory        | Purpose                                                     |
|------------------|-------------------------------------------------------------|
| `include/`       | All `.h` header files — class declarations, inline methods  |
| `src/`           | All `.cpp` files — implementations, `main.cpp`, src Makefile|
| `srcInterface/`  | SWMF coupling: `PC_wrapper.f90`, `PT_wrapper.f90`, `FleksInterface.cpp` |
| `Doc/`           | User manual, algorithms, coding standards, and agent docs |
| `tools/`         | `amrex2tec.py`, `amrex2vtk.sh`, `converter.py`, `generate_compile_commands.py` |

---

## Architecture

### Class Hierarchy

```
Domain                        — Top-level simulation manager
 ├── DomainGrid               — Grid information container
 ├── Pic : Grid : AmrCore     — PIC solver (fields + particle push on AMR grid)
 ├── ParticleTracker : Grid   — Test particle tracker
 ├── FluidInterface : Grid    — MHD/fluid state on grid (coupling data)
 ├── SourceInterface          — Source terms + ionization parameters
 │    └── UserSource          — User-defined source (exosphere ionization)
 ├── OHInterface              — Outer-heliosphere coupling data
 └── TimeCtr                  — Time stepping, event control, plot scheduling
      ├── EventCtr            — Periodic event trigger (dn or dt based)
      └── PlotCtr             — Plot scheduling (combines EventCtr + PlotWriter)
```

Key design principle: ionization parameters live in **SourceInterface** (not
FluidInterface), and the physics is implemented in **UserSource** (ExoSource.h).
This keeps FluidInterface uncluttered for MHD coupling data only.

### Core Classes

| Class              | Header              | Purpose                                                   |
|--------------------|----------------------|-----------------------------------------------------------|
| `Domain`           | `Domain.h`           | Top-level orchestrator: owns Pic, FluidInterface, TimeCtr |
| `Pic`              | `Pic.h`              | PIC solver: field solve, particle push, moments, I/O; hybrid path uses cell-centered fields |
| `Grid`             | `Grid.h`             | AMR grid management (inherits `AmrCore`)                  |
| `Particles`        | `Particles.h`        | Templated particle container (PIC or PT particles)        |
| `TestParticles`    | `TestParticles.h`    | Test particle class (trajectory recording, I/O)           |
| `FluidInterface`   | `FluidInterface.h`   | Fluid/MHD state variables on PIC grid                     |
| `LinearSolver`     | `LinearSolver.h`     | GMRES Krylov solver for implicit E-field                  |
| `TimeCtr`          | `TimeCtr.h`          | Time step management, CFL, event scheduling               |
| `PlotWriter`       | `PlotWriter.h`       | Output formatting (IDL, AMReX, HDF5, VTK, Tecplot)        |
| `DataContainer`    | `DataContainer.h`    | Data reading (IDL format, binary/ascii)                   |
| `GridUtility`      | `GridUtility.h`      | Discrete operators (curl, div, grad, averaging)           |
| `BC`               | `BC.h`               | Boundary conditions (periodic, coupled, outflow, vacuum)  |
| `Constants`        | `Constants.h`        | Physical/numerical constants, particle record sizes       |

### SWMF Interface Layer (`srcInterface/`)

| File                  | Purpose                                                    |
|-----------------------|------------------------------------------------------------|
| `PC_wrapper.f90`      | Fortran wrapper for PC component (called by SWMF coupler)  |
| `PT_wrapper.f90`      | Fortran wrapper for PT component                           |
| `FleksInterface.cpp`  | C++ entry points called by Fortran wrappers via `bind(C)`  |

The interface layer follows a pattern:
1. SWMF calls `PC_wrapper.f90` subroutines (e.g., `PC_run`, `PC_put_from_gm`)
2. Fortran wrapper calls C functions in `FleksInterface.cpp` (e.g., `fleks_run_`)
3. C++ functions operate on the global `fleksDomains` object

### Algorithm Summary

- **PIC Method:** Semi-implicit θ-scheme with Boris particle mover
- **Hybrid PIC Method:** Kinetic ions + massless fluid electrons, advanced via
  a generalized Ohm's law and an explicit cell-centered Faraday update (RK4 or
  SSPRK3), gated by `#HYBRIDPIC` (`useHybridPIC`). See `Doc/Algorithm.tex`.
- **Field Solver:** GMRES iterative solver for the implicit electric field
  (full-PIC); the hybrid path instead assembles E from the generalized Ohm's law
  (`Pic::assemble_ohm_E`).
- **Time Stepping:** CFL-based or fixed Δt; configurable θ parameter (default 0.51)
- **Divergence Cleaning:** Accurate div(E) correction via particle position adjustment; optional hyperbolic div(B) cleaning
- **Particle Management:** Splitting, merging, fast merge algorithm with Lagrange multipliers
- **AMR:** Block-structured AMR via AMReX with configurable refinement levels and regions

---

## Build System

FLEKS has two main build targets. `make LIB` builds the SWMF component library
and wrappers; `make EXE` builds the standalone `bin/FLEKS.exe` driver.

`Config.pl` chooses the locations of `share`, `util`, `lib`, and AMReX. When
FLEKS is checked out under `SWMF/PC/FLEKS`, these normally come from the parent
SWMF tree; in a pure FLEKS checkout, `Config.pl` can clone/use local
dependencies. Only `make EXE` links the standalone driver; source compilation
uses the same component preprocessor flag as `make LIB`.

### Dependencies

| Dependency | Required | Notes                                      |
|------------|----------|--------------------------------------------|
| AMReX      | Yes      | `../../util/AMREX/InstallDir/` in SWMF, or `util/AMREX/InstallDir/` standalone |
| MPI        | Yes      | `mpicxx`/`mpif90` must be in PATH          |
| SWMF share | Yes      | Provides `share/Scripts`, `share/Library`, and `libSHARE.a` |
| HDF5       | Optional | Parallel HDF5 for HDF5 output support      |
| Perl       | Yes      | For `Config.pl` and `share/Scripts/`        |

### Build Targets

```bash
# Standalone executable
make EXE -j8

# Library for SWMF integration
make LIB -j8

# Converter tool
make CONVERTER

# Regenerate compile_commands.json (for IDE support)
make compile_commands

# Clean
make clean       # Object files only
make distclean   # Full reset
```

### Configuration

```bash
# Setup/configuration
./Config.pl             # Show current settings when no args are given
./Config.pl -install    # Generate Makefile.conf if needed
./Config.pl -tp=PBE     # Set test particle output (P, PB, PBE, PBEG)
./Config.pl -lev=2      # Set max AMR levels
```

Inside an SWMF tree, bare `./Config.pl -install` configures FLEKS to reuse the
parent SWMF paths.

### Build Artifacts

Typical outputs after building:
- `bin/FLEKS.exe` (standalone executable)
- `src/libFLEKS.a` (static library for linking)
- `srcInterface/lib*.a` / wrapper objects for SWMF component mode
- `compile_commands.json` (IDE index)

If these are missing, run the build targets in the **Build Targets** section above.

---

## Coding Conventions

See `Doc/Coding_standards.md` for the full list. Key points:

### Naming

| Element        | Convention       | Example                    |
|----------------|------------------|----------------------------|
| Files          | PascalCase       | `GridUtility.cpp`          |
| Classes        | PascalCase       | `FluidInterface`           |
| Functions      | snake_case       | `apply_field_bc()`         |
| Variables      | camelCase        | `nCellPerPatch`            |
| Constants      | camelCase/UPPER  | `cProtonMassSI`, `nDim3`   |
| Private members| camelCase        | `doRestart`                |

### Style Rules

1. **Smart pointers** — Use `unique_ptr`/`shared_ptr` for ownership; raw
   pointers only when there is no ownership
2. **`using namespace amrex`** — Allowed only in `.cpp` files, never in headers
3. **Header order** — std → AMReX → project headers
4. **`nullptr`** over `NULL`
5. **`const`** wherever possible
6. **80-column limit** — Enforced by `.clang-format` (Mozilla-based style)
7. **Lambdas** — OK for short/local use; prefer named functions for reusable logic
8. **Commits** — Follow [conventional commits](https://www.conventionalcommits.org/)
9. **Include guards** — `#ifndef _FILENAME_H_` / `#define _FILENAME_H_` pattern

### Formatting

**C++ Formatting:**
The project uses Clang-Format with a Mozilla-based configuration. Key settings:
- `IndentWidth: 2`
- `ColumnLimit: 80`
- `BreakBeforeBraces: Attach`
- `UseTab: Never`

**Fortran Formatting:**
For `.F90`/`.f90` files, use `findent` configured to match Emacs' `f90-mode` indentation. This is important to remember when modifying Fortran source files in `srcInterface/` or other components.

---

## Parameter System

Simulation parameters are controlled via `PARAM.in` files using the SWMF
parameter reading system. All supported commands are documented in `PARAM.XML`.

### Key Command Groups

| Group                        | Example Commands                                   |
|------------------------------|----------------------------------------------------|
| **Output**                   | `#SAVEPLOT`, `#MONITOR`, `#SAVELOG`, `#NOUTFILE`   |
| **Scheme**                   | `#PIC`, `#TIMESTEPPING`, `#DISCRETIZATION`, `#EFIELDSOLVER`, `#DIVE`, `#DIVB` |
| **Hybrid PIC**               | `#HYBRIDPIC`, `#RESISTIVITY`, `#HALLTERM`, `#ELECTRONTEMPERATURE`, `#HYPERRESISTIVITY`, `#BSUBCYCLE`, `#MINIMUMDENSITY`, `#FIELDINTEGRATOR`, `#AVGFIELDB`, `#SMOOTHMOMENTS` |
| **Particles**                | `#PARTICLES`, `#RESAMPLING`, `#FASTMERGE`, `#VACUUM`, `#PARTICLETRACKER` |
| **Initial/Boundary Cond.**   | `#GEOMETRY`, `#NCELL`, `#REGION`, `#PARTICLES`, `#BC` |
| **Coupling**                 | `#OHION`, `#CHARGEEXCHANGE`, `#MAXCHARGEEXCHANGERATE` |

---

## Testing & Running

When coupled with SWMF, FLEKS is initialized and driven by the SWMF framework.
For standalone use, `src/main.cpp` initializes AMReX, reads `PARAM.in`, creates
a `Domain`, calls `set_ic()`, and advances until `#STOP` criteria are met.
Standalone inputs that do not use SWMF should set `#INITFROMSWMF` to `F` and
provide initial state commands such as `#NORMALIZATION`, `#PLASMA`, and
`#UNIFORMSTATE`.

The standalone driver currently uses domain name `FLEKS1`; plots and restarts
are written under `FLEKS1/`.

### CI

The GitHub Actions workflow runs the SWMF regression tests, builds standalone
`bin/FLEKS.exe`, and runs the test suite under `tests/` via
`python3 tests/validate_tests.py`.

### Standalone Test Suite

The standalone test suite is organized by physics scenario:

| Test                  | Directory                | Physics                              |
|-----------------------|--------------------------|--------------------------------------|
| Beam instability      | `tests/beam/`            | Ion beam cyclotron wave growth; run with both solvers (full PIC + hybrid) |
| Photoionization       | `tests/photoionization/` | Chamberlain profile, photoionization |
| Electron impact       | `tests/electronimpact/`  | Voronov-1997 impact rate, e- only    |
| Charge exchange       | `tests/chargeexchange/`  | Constant CX cross section, ions only |
| Whistler wave         | `tests/whistler/`        | Whistler-Alfven wave; full PIC + hybrid (Hall only) |
| Hybrid Ohm's law      | `tests/ohm/`             | Full generalized Ohm's law (convection + Hall + resistive + e-pressure) |
| Ion acoustic wave     | `tests/iaw/`             | IAW; electron-pressure (ambipolar) branch + ion Landau damping |
| Free-stream           | `tests/freestream/`      | 1D uniform equilibrium; full PIC + hybrid (Hall off) |
| Light wave            | `tests/lightwave/`       | 3D vacuum transverse wave on periodic AMR grid |
| PCAI                  | `tests/pcai/`            | Proton-cyclotron anisotropy instability (hybrid benchmark) |
| Hyper-resistivity     | `tests/hyper_resistivity/` | 4th-order `-eta_h nabla^2 J` Ohm term (smoke test) |
| Single-cell Hall      | `tests/singlecell/`      | 1-cell periodic; curl B = 0 => Hall term exactly 0 |
| Zero-current wave     | `tests/zerocurrent/`     | No particles (J=0); Hall term vanishes, wave frozen |
| Performance benchmark | `tests/performance/`     | Beam-based scaling benchmark; hybrid variant included |

Each test directory contains a `PARAM.in`, a `README.md`, and (for most tests)
its own `validate.py` module holding that test's validators (energy-log checks,
plot-output checks, and optionally a `PARTICLE_TOL` dict enabling the
test-particle tracer check). The unified runner `tests/validate_tests.py` keeps
only the shared infrastructure (run-directory management, launching FLEKS and
PostProc, reading the PIC/particle logs, the summary table) and dynamically
loads the `validate.py` module for whichever test it is about to run, so a
single test does not import the code for the others. Code shared by several
tests (e.g. the hybrid family) lives in `tests/_shared/`. Output is controlled
with the Python `logging` module (INFO by default; pass `--verbose`/`-v` for
DEBUG diagnostics). Results are written to `tests/summary.md`.

A test directory may ship a `PARAM.in.hybrid` variant (e.g. `beam/`,
`freestream/`, `whistler/`, `performance/`). When present, the runner executes
the test once per field solver and lists both variants in the summary table
(e.g. `BEAM` and `BEAM (HYBRID)`).

Ionization processes are configured via separate parameter commands:
- `#PHOTOIONIZATION` — geometric \\(1/r^2\\) dilution
- `#ELECTRONIMPACT` — Voronov 1997 rate coefficient
- `#CHARGEEXCHANGE` — constant cross-section model

### Tools & Post-Processing

| Script                      | Purpose                                |
|-----------------------------|----------------------------------------|
| `tools/amrex2tec.py`       | Convert AMReX output to Tecplot format |
| `tools/amrex2vtk.sh`       | Convert AMReX output to VTK format     |
| `tools/converter.py`       | General data conversion                |
| `tools/generate_compile_commands.py` | Generate `compile_commands.json` |

**Visualization:** While ParaView can read HDF5 data natively, it may struggle with continuous rendering of AMReX block boundaries unless you use the `BATSRUSReader` ParaView plugin provided by the `flekspy` Python toolkit. Alternatively, converting output to `.vtm` or Tecplot formats using the tools above is often preferred. For Python scripting, the official tool is the **`flekspy`** package (`pip install flekspy`), which natively parses FLEKS AMReX data and integrates with Matplotlib and YT.

---

## AI Agent Skills

To assist with common tasks, specialized instructions (skills) are defined in `.agent/skills/`. You are encouraged to view these Markdown files when performing related tasks:

| Skill             | Description                                          | Path                                           |
|-------------------|------------------------------------------------------|------------------------------------------------|
| **Build FLEKS**   | Compile project and regenerate compile_commands.json | `.agent/skills/build-fleks/SKILL.md`           |
| **Add New Source**| Create new `.cpp`/`.h` files following conventions   | `.agent/skills/add-new-source/SKILL.md`        |
| **Code Cleanup**  | Formatting, unused variables, standard checks        | `.agent/skills/code-cleanup/SKILL.md`          |
| **Debug Session** | Setup and launch a debug session (lldb / VS Code)    | `.agent/skills/debug-session/SKILL.md`         |
| **Generate Docs** | Build HTML/PDF documentation                         | `.agent/skills/generate-docs/SKILL.md`         |
| **Run Test**      | Run regression tests via SWMF test infrastructure    | `.agent/skills/run-test/SKILL.md`              |

## Agent Workflows

Composite step-by-step guides for multi-stage tasks are in `.agent/workflows/`:

| Workflow                  | Description                                          | Path                                        |
|---------------------------|------------------------------------------------------|---------------------------------------------|
| **Run Test**              | Run test16_3d regression test end-to-end             | `.agent/workflows/run-test.md`              |
| **Add Parameter Command** | Add a new `#COMMANDNAME` from PARAM.XML to code      | `.agent/workflows/add-param.md`             |
| **Add Coupling Variable** | Add a new GM↔PC exchange variable for SWMF coupling  | `.agent/workflows/add-coupling-var.md`      |

---

## Common Workflows

### Adding a New Source File

1. Create `include/NewFeature.h` with include guards
2. Create `src/NewFeature.cpp`
3. Add `NewFeature.cpp` to `SRCS` in `src/Makefile`
4. Build: `make LIB -j8`
5. If the standalone executable needs the new code path, also run `make EXE -j8`

### Adding a New Parameter Command

1. Document in `PARAM.XML`
2. Parse in the relevant `read_param()` method (usually in `Domain.cpp` or `Pic.cpp`)
3. Add the corresponding member variable

### Adding a New Test Case (Initial Condition)

Test-case initial conditions are plug-ins resolved by name from `#TESTCASE`
through the `InitialCondition` registry (`include/InitialCondition.h`,
`src/ic/RegisterAll.cpp`). The kernel names **zero** test cases — an unknown
`#TESTCASE` name aborts loudly listing the registered names, so a typo can
never silently fall back to a uniform plasma.

**Wave / sinusoidal tests cost zero C++.** All transverse and sinusoidal wave
seeds are ONE parameterized plug-in (`WaveIC`, `src/ic/WaveIC.cpp`), registered
under the legacy aliases `lightwave`, `hybridwave`, `convectionwave`,
`ionacousticwave` **and** a generic `waveic`. To add a new wave test, write a
`#TESTCASE waveic` (or one of the aliases) plus a `#WAVEIC` block in the test's
`PARAM.in` — no C++ needed. `#WAVEIC` sub-parameters (all optional;
`read_optional` so absent ones keep the preset):

- `seedE` / `seedB` / `seedWeight` (logical): seed E field / B field / sinusoidal
  density perturbation (weight scaling `1 + pert*sin(kx*x)`).
- `oblique` (logical) + `dir 0.6 0.8 0` + `waveLength 48`: oblique plane wave
  with the given propagation direction and wavelength in cells (LightWave uses
  this). Otherwise the wave is x-aligned with `kx = 2*pi*waveMode/Lx`.
- `guideField` (logical): add the uniform guide field `Bx0` (from `#UNIFORMSTATE`)
  under the B perturbation (`B = Bx0 + B1*cos kx, B1*sin kx`, `B1 = frac*Bx0`).
- `velKick` (logical): matching Alfven ion velocity kick
  (`delta U_y = -B1 cos kx`, `delta U_z = -B1 sin kx`).
- `frac` (real): B perturbation amplitude `B1/Bx0`.
- `pert` (real): density perturbation amplitude (`seedWeight`).
- `waveMode` (int): mode number for the x-aligned wavenumber.

**Non-wave tests** (beam, tophat) keep dedicated plug-ins in `src/ic/`. To add a
brand-new *non-wave* test, subclass `InitialCondition` (override `read_param`,
`set_fields`, the per-particle `modify_particle_weight` / `modify_particle_velocity`
hooks, and `name()`), register it in `src/ic/RegisterAll.cpp`, and add the `.cpp`
to `SRCS` in `src/Makefile`.

### Modifying the SWMF Interface

1. Add C++ function in `srcInterface/FleksInterface.cpp`
2. Add Fortran wrapper in `PC_wrapper.f90` or `PT_wrapper.f90`
3. Ensure C-compatible calling convention (`extern "C"` or `bind(C)`)

### Modifying Standalone Behavior

1. Update `src/main.cpp` for driver-level behavior.
2. Keep `Domain` logic shared with SWMF whenever possible.
3. Build with `make EXE -j8`.
4. Run from a directory containing `PARAM.in`, or from the standalone test
   directory if adding/changing a test fixture.

---

## Important Notes

- FLEKS is always 3D internally. 2D is achieved with a single cell in the
  z-direction ("fake 2D").
- The `Constants.h` file is generated from `Constants.h.orig` and should not be
  edited directly — use `Config.pl` to change settings like `ptRecordSize` or
  `nLevMax`.
- `UserSource.h` is similarly copied from `UserSource.h.orig` and is intended
  for user-customizable source terms.
- The `show_git_info.h` file is auto-generated at build time with git revision
  information.
- Source builds define `_<COMPONENT>_COMPONENT_`; for FLEKS this is
  `_PC_COMPONENT_`.

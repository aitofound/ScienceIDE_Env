# src/ — FLEKS Implementation Files

All C++ implementation files (`.cpp`) and the source-level Makefile live here.
Headers are in `include/`.

## File Overview

| File                        | Implements            | Description                                                  |
|-----------------------------|-----------------------|--------------------------------------------------------------|
| `main.cpp`                  | `main()`              | Standalone entry point. Initializes AMReX, prints git info. Currently the `Domain` init is commented out — FLEKS is primarily run via the SWMF coupler. |
| `Pic.cpp`                   | `Pic`                 | Core PIC solver: field update loop, particle push, moment deposition, E-field solve (GMRES), div(E) cleaning, field smoothing. |
| `PicIO.cpp`                 | `Pic` (I/O methods)   | Plot output, restart save/read, AMReX/IDL/HDF5 writing for `Pic`. |
| `Particles.cpp`             | `Particles<N,M>`      | Particle operations: injection, movement (Boris pusher), splitting, merging, fast merge, charge exchange, boundary handling. |
| `TestParticles.cpp`         | `TestParticles`       | Test particle movement, trajectory recording, I/O.           |
| `ParticleTracker.cpp`       | `ParticleTracker`     | Test particle manager: initialization, stepping, output scheduling. |
| `Domain.cpp`                | `Domain`              | Top-level simulation flow: `init()`, `update()`, parameter reading, restart, regridding, coupling data exchange. |
| `FluidInterface.cpp`        | `FluidInterface`      | Fluid state management: set/get MHD variables, unit conversion, moment interpolation, grid operations. |
| `Grid.cpp`                  | `Grid`                | AMR grid operations: regridding, cell status, load balancing, node grid computation. |
| `GridUtility.cpp`           | Free functions         | Discrete operators (curl, div, grad), array averaging, printing utilities. |
| `LinearSolver.cpp`          | `LinearSolver`, `gmres()` | GMRES iterative solver implementation, Krylov methods.    |
| `PlotWriter.cpp`            | `PlotWriter`          | Output file generation: IDL binary/ascii, Tecplot, VTK formats. |
| `DataContainer.cpp`         | `DataContainer` hierarchy | Data reading for AMReX format files.                     |
| `VisitWriter.cpp`           | VTK functions          | VisIt/VTK unstructured mesh writer (adapted from LLNL code). |
| `TimeCtr.cpp`               | `EventCtr`            | Event timing logic (is_time_to checks).                      |
| `BC.cpp`                    | Boundary conditions    | Float boundary application.                                 |
| `Converter.cpp`             | `Converter`           | Standalone data format conversion tool.                      |
| `FleksDistributionMap.cpp`  | `FleksDistributionMap`| Custom MPI distribution mapping.                             |

## Build

The `src/Makefile` compiles all `.cpp` files listed in `SRCS` into
`libFLEKS.a`. The standalone executable links `main.o` against this library.

### Adding a new source file

1. Create `src/NewFile.cpp`
2. Add `NewFile.cpp` to the `SRCS` variable in `src/Makefile`
3. Run `make LIB -j8` from the project root

### Key Makefile variables

| Variable       | Value                     | Purpose                    |
|----------------|---------------------------|----------------------------|
| `SRCS`         | List of `.cpp` files      | Source files for `libFLEKS.a` |
| `SEARCH_C`     | `-I../include -I... `     | Include search paths       |
| `FLAGC_EXTRA`  | `-D_${COMPONENT}_COMPONENT_` | Component preprocessor flag|
| `LIBFLEKS`     | `libFLEKS.a`              | Output library name        |

## Preprocessor Flags

| Flag                  | Effect                                        |
|-----------------------|-----------------------------------------------|
| `_PC_COMPONENT_`      | Compile as PC (Particle-in-Cell) component    |
| `_PT_COMPONENT_`      | Compile as PT (Particle Tracker) component    |
| `_USE_HDF5_`          | Enable HDF5 output support                    |
| `_AMR_DEV_`           | AMR development features (commented out)      |

## Development Note

- **Environment:** The primary development environment is **macOS**. Keep this in mind when troubleshooting build steps or configuring debug sessions (e.g., standardize on `lldb` for debugging in terminal or VS Code).

## Validation

- Fast compile check after source edits: `make LIB -j8`
- Executable link check (when touching `main.cpp` or top-level linking): `make FLEKS -j8`
- If new files are added, confirm `src/Makefile` `SRCS` is updated before rebuilding.

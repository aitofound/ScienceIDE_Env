# include/ — FLEKS Header Files

All public header files (`.h`) for the FLEKS project live in this directory.
Implementation files are in `src/`. Headers should never contain
`using namespace` statements.

## File Overview

### Core Simulation

| File                | Class/Contents                  | Description                                              |
|---------------------|---------------------------------|----------------------------------------------------------|
| `Pic.h`             | `Pic`, `FieldSolver`           | Main PIC solver — field updates, particle push, moments, I/O. Inherits `Grid`. |
| `Domain.h`          | `Domain`                       | Top-level simulation manager. Owns `Pic`, `FluidInterface`, `TimeCtr`. |
| `DomainGrid.h`      | `DomainGrid`                   | Grid-level domain info container.                        |
| `Domains.h`         | `Domains` (typedef)            | Collection of `Domain` objects for multi-domain runs.    |
| `Grid.h`            | `Grid`                         | AMR grid management. Inherits `AmrCore`. Cell/node grids, regridding, load balancing. |
| `Constants.h`       | Constants, enums                | Physical constants, index aliases, particle struct sizes. **Auto-generated from `.orig`.** |
| `TimeCtr.h`         | `TimeCtr`, `EventCtr`, `PlotCtr` | Time stepping, CFL, periodic event scheduling, plot control. |

### Particles

| File                | Class/Contents                   | Description                                              |
|---------------------|----------------------------------|----------------------------------------------------------|
| `Particles.h`       | `Particles<N,M>`, `PicParticles`, `PTParticles` | Templated particle container. PIC and test-particle specializations. Boris mover, splitting/merging. |
| `TestParticles.h`   | `TestParticles`                  | Test particle tracker. Records trajectories, supports relativistic movers. |
| `ParticleTracker.h` | `ParticleTracker`                | Manages test particle populations.                       |

### Fluid / Coupling Interface

| File                  | Class/Contents                  | Description                                              |
|-----------------------|---------------------------------|----------------------------------------------------------|
| `FluidInterface.h`   | `FluidInterface`, `FluidInterfaceParameters` | MHD/fluid state on PIC grid. Unit conversion, moment exchange. Inherits `Grid`. |
| `FleksInterface.h`   | Function declarations           | C-callable interface functions for SWMF Fortran wrappers.|
| `SWMFInterface.h`    | SWMF helpers                    | SWMF-specific coupling utilities.                        |
| `OHInterface.h`      | `OHInterface`                   | Outer-heliosphere (OH) coupling data container.          |
| `OHSource.h`         | OH source terms                | Helper interfaces for outer-heliosphere source handling. |
| `SourceInterface.h`  | `SourceInterface`               | Source term interface for coupled simulations.            |
| `UserSource.h`       | User source hooks                | User-customizable source terms. **Auto-generated from `.orig`.** |

### Fields & Solvers

| File                | Class/Contents                   | Description                                              |
|---------------------|----------------------------------|----------------------------------------------------------|
| `LinearSolver.h`    | `LinearSolver`, `LinearSolverParam` | GMRES Krylov iterative solver for implicit E-field.     |
| `GridUtility.h`     | Free functions                   | Discrete operators: curl, div, grad, averaging, interpolation. |
| `UInterp.h`         | Interpolation utilities          | Node/cell interpolation routines.                        |
| `BC.h`              | `BC`                             | Boundary condition types (periodic, coupled, outflow, vacuum). |

### Data & I/O

| File                | Class/Contents                   | Description                                              |
|---------------------|----------------------------------|----------------------------------------------------------|
| `PlotWriter.h`      | `PlotWriter`                     | Output formatting for IDL, AMReX, HDF5, VTK, Tecplot.   |
| `DataContainer.h`   | `DataContainer`, `IDLDataContainer`, `AMReXDataContainer` | Data readers for various file formats. |
| `DataWriter.h`      | `DataWriter`                     | Base writer interface.                                   |
| `VisitWriter.h`     | VTK writing functions            | VisIt/VTK unstructured mesh writer (from LLNL).          |
| `Converter.h`       | `Converter`                      | Data format conversion utilities.                        |

### Utility

| File                | Class/Contents                   | Description                                              |
|---------------------|----------------------------------|----------------------------------------------------------|
| `Utility.h`         | Free utility functions           | String parsing, math helpers, general utilities.         |
| `Array1D.h`         | `Array1D`                        | Simple 1D array wrapper.                                 |
| `Bit.h`             | Bit manipulation                 | Cell status bit flags (active, new, boundary, etc.).     |
| `BitArray.h`        | `BitArray`                       | Compact boolean array.                                   |
| `Morton.h`          | Morton curve                     | Space-filling curve for grid ordering.                   |
| `Regions.h`         | `Regions`                        | Named geometric regions (box, sphere, shell, paraboloid).|
| `Shape.h`           | `Shape`                          | Geometric shape definitions for region specification.    |
| `Delauator.h`       | Delaunay triangulation           | Delaunay triangulation utility.                          |
| `UMultiFab.h`       | `UMultiFab`                      | Extended MultiFab utilities.                             |
| `Timer.h`           | `Timer`                          | Simple timing utility.                                   |
| `FleksDistributionMap.h` | `FleksDistributionMap`      | Custom AMReX distribution mapping for load balancing.    |
| `ReadBATL.h`        | BATL reading                     | Read BATS-R-US grid data.                                |

### Auto-Generated (Do Not Edit Directly)

| File                    | Generated From           | Controlled By         |
|-------------------------|--------------------------|-----------------------|
| `Constants.h`           | `Constants.h.orig`       | `Config.pl -tp=`, `-lev=` |
| `UserSource.h`          | `UserSource.h.orig`      | `make include/UserSource.h` / Copy |
| `show_git_info.h`       | `show_git_info.h.orig`   | Build system (`gitall` script) |

> **Note:** If you need to make permanent changes to mathematical or physics constants, edit `Constants.h.orig` and NOT `Constants.h`. Changes to `Constants.h` will be overwritten when `make` operates or `Config.pl` runs.

## Header Include Order Convention

```cpp
// 1. Standard library
#include <iostream>
#include <vector>

// 2. AMReX
#include <AMReX.H>
#include <AMReX_MultiFab.H>

// 3. Project headers
#include "Grid.h"
#include "Utility.h"
```

## Include Guard Convention

```cpp
#ifndef _FILENAME_H_
#define _FILENAME_H_
// ...
#endif
```

## Validation

- After header edits, run `make LIB -j8` from repo root to catch signature or include-order issues.
- If generated headers are involved (`Constants.h`, `UserSource.h`), validate the corresponding `.orig` source before rebuilding.

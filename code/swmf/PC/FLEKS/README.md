# FLEKS — FLexible Exascale Kinetic Simulator

FLEKS is a highly-optimized Particle-In-Cell (PIC) and particle tracker code built on top of the [AMReX](https://amrex-codes.github.io/) block-structured adaptive mesh refinement framework. 

It natively serves as the **PC** (Particle-in-Cell) and **PT** (Particle Tracker) components within the [SWMF](https://github.com/SWMFsoftware) (Space Weather Modeling Framework), heavily coupling with BATS-R-US via the MHD-AEPIC algorithm to provide hybrid kinetic resolution to global MHD models.

## Primary Capabilities
- **Implicit PIC:** Semi-implicit $\theta$-scheme solvers leveraging AMReX GMRES and heavily optimized particle-push boundaries.
- **Hybrid PIC:** Kinetic ions + massless fluid electrons via a generalized Ohm's law, with an explicit RK4/SSPRK3 Faraday advance (see `Doc/Algorithm.tex`).
- **Particle Tracking:** Massively parallel test particle tracking in turbulent MHD and PIC electromagnetic fields.
- **MHD-AEPIC:** True bi-directional coupling between global space weather MHD states and sub-grid PIC regimes.
- **Adaptive Tracing:** Utilizes AMReX regridding and dynamic load balancing.

## Build Requirements
FLEKS is designed to be built continuously alongside SWMF. See the [SWMF documentation](https://github.com/SWMFsoftware) for environment requirements.
- CMake (Optional, for standalone builds)
- C++14/17 compatible compiler (e.g., `g++` 8+, `clang++` 9+, or `icpc`)
- Fortran 90 compiler (e.g., `gfortran`, `ifort` for `srcInterface/`)
- MPI (MPICH or OpenMPI)

## Quick Start

### Via SWMF
```bash
# Inside the SWMF root directory
make test16_3d_compile
make test16_3d
```

### Standalone Tests
FLEKS can also be built and run standalone (no SWMF) using the tests under `tests/`:

```bash
# From the FLEKS root directory
./Config.pl -lev=2 -u=Exo   # 2 AMR levels + Exosphere source
make -j4
python3 tests/validate_tests.py --test=singlecell   # run a single test
```

See `tests/README.md` for the full test catalogue and runner options.

## Documentation
### Parameter reference manual

The input commands and parameters are documented in `PARAM.XML`. After
configuring FLEKS, users with command-line LaTeX (`pdflatex` and `makeindex`)
plus the `fvextra` package can build the PDF reference manual with:

```bash
make PDF
```

The generated manual is `Doc/USERMANUAL.pdf`.

To remove intermediate documentation files while preserving the PDF, run:

```bash
make -C Doc/Tex clean
```

To remove the intermediates and the generated PDF, run:

```bash
make -C Doc/Tex cleanpdf
```

* **Algorithms & Physics:** Please see `Doc/Algorithm.tex` for the mathematical foundations of the simulation engine.
* **Component Architecture:** Every core directory contains an extensive `AGENT.md` file describing layout, file purposing, and structure.
* **Coding Standards & Contributing:** See `CONTRIBUTING.md` before making a pull request.

## Data Processing
All outputs from FLEKS natively stream to AMReX block formats. Do not use generic NetCDF loaders. Please leverage the Python `flekspy` API via:
```bash
pip install flekspy
```

## License
Apache 2.0 (University of Michigan)

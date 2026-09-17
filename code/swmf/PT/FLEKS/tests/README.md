# FLEKS Tests

This directory contains standalone tests for the FLEKS (Flexible Exascale Kinetic
Simulator) particle-in-cell (PIC) solver, independent of SWMF coupling.

## Directory Structure

Each test case is contained within its own dedicated subdirectory containing a
`PARAM.in` configuration file and a README file:

| Test                  | Dir                   | Description                                                  | Readme |
|-----------------------|-----------------------|--------------------------------------------------------------|--------|
| Beam instability      | `beam/`               | 1D ion-beam cyclotron instability; energy conservation        | [README](beam/README.md) |
| Photoionization       | `photoionization/`    | Chamberlain neutral atmosphere with photoionization           | [README](photoionization/README.md) |
| Electron impact       | `electronimpact/`     | Voronov 1997 electron-impact ionization of hot electrons      | [README](electronimpact/README.md) |
| Charge exchange       | `chargeexchange/`     | Constant-cross-section charge exchange with solar wind ions   | [README](chargeexchange/README.md) |
| Whistler wave         | `whistler/`           | Whistler–Alfven wave (Hall term)                              | [README](whistler/README.md) |
| Ohm's law             | `ohm/`                | Full generalized Ohm's law check | [README](ohm/README.md) |
| Free-stream           | `freestream/`         | 1D uniform free-stream                                        | [README](freestream/README.md) |
| Light wave            | `lightwave/`          | 3D vacuum light wave on a periodic AMR grid | [README](lightwave/README.md) |
| PCAI                  | `pcai/`               | Proton-cyclotron anisotropy instability (`T_perp/T_par=3`, `gamma/Omega_ci=0.162`) | [README](pcai/README.md) |
| Reconnection          | `reconnection/`       | Fadeev current-sheet reconnection on a uniform grid (x-y)     | [README](reconnection/README.md) |
| AMR reconnection      | `reconnection_amr/`   | Fadeev current-sheet reconnection on a two-level AMR grid | [README](reconnection_amr/README.md) |
| Reflecting & PEC BC   | `bc_reflecting/`      | Specular reflecting particle walls + conducting (PEC) field walls (4 variants: full/hybrid fields/particles) | [README](bc_reflecting/README.md) |
| Absorbing BC          | `bc_absorb/`          | Absorbing field + particle boundaries (4 variants: full/hybrid fields/particles) | [README](bc_absorb/README.md) |
| Wave injection        | `bc_wave/`            | Grouped wave-injection tests: mono Bz wave + shear Alfvén wave via `#WAVEBC` (one `PARAM.in.<suffix>` per variant) | [README](bc_wave/README.md) |
| Oblique shock         | `shock/`              | 1D oblique magnetized shock | [README](shock/README.md) |
| Performance           | `performance/`        | Beam-based scaling benchmark         | — (see `validate_performance.py`) |

### Ionization Parameter Commands

Each ionization process is enabled via a dedicated command in PARAM.in:

- **`#PHOTOIONIZATION`**: per-component rates at planet surface [s^-1],
  diluted geometrically as `(rPlanet / r)^2`
- **`#ELECTRONIMPACT`**: Voronov 1997 formula: `sigmav(T) = A*(T/EI)^K / [X+(T/EI)] * exp(-EI/T)` [cm^3/s], parameters per component
- **`#CHARGEEXCHANGE`**: constant cross-section: `sigmav(u) = sigmaCX * |u_i|` [cm^3/s], cross-section matrix `sigmaCX(neutral, ion)` [cm^2]; each neutral component exchanges with all ion species and the frequency is summed over ions

## Architecture

Ionization parameters are stored in `SourceInterface` and read by `UserSource::read_param()` in `userfiles/ExoSource.h`. The Domain routes specific commands to the source object rather than to `FluidInterface`. This keeps the MHD coupling layer uncluttered by ionization-specific data.

## Building the Test Executable

### 3D AMReX build (default)

Most standalone tests run with a **single FLEKS executable** built with the
Exosphere user source (`-u=Exo`, needed by the ionization tests) and two grid
levels (`-lev=2`, needed by the AMR tests).  This is the default build and links
the **3D** AMReX library:

```bash
cd PC/FLEKS            # (or the FLEKS root)
./Config.pl -lev=2 -u=Exo
make -j4
```

### 2D AMReX build

The suite can also be built against the **true-2D AMReX library**
(`AMReX_SPACEDIM = 2`):

```bash
cd PC/FLEKS            # (or the FLEKS root)
./Config.pl -amrex2d -lev=2 -u=Exo
make -j4
```

The runner auto-detects the built AMReX dimension and skips any test that needs
the other one (e.g. the true-3D `lightwave` and the source-term /
hyper-resistivity tests under a 2D build).  Use `-amrex3d` to switch back to the
default 3D build.

## Running the Tests

### Standard Test Suite

A unified Python runner is provided to dynamically discover, run, and validate
the standalone tests:

```bash
# Run all tests in serial mode (no MPI):
python3 tests/validate_tests.py

# Run a single test by name (e.g. beam):
python3 tests/validate_tests.py --test=beam

# Equivalently, with a space:
python3 tests/validate_tests.py --test beam

# Run with N MPI processes:
python3 tests/validate_tests.py -n 2
python3 tests/validate_tests.py --nprocs 2

# Show detailed per-check diagnostics (energy numbers, ratios, etc.):
python3 tests/validate_tests.py --verbose
python3 tests/validate_tests.py -v
```

When `-n 1` (or the flag is omitted), the executable is invoked directly as
`./FLEKS.exe` without `mpirun`. When `-n N` with `N > 1`, it uses
`mpirun -n N ./FLEKS.exe`.

The `--test NAME` (or `--test=NAME`) option selects a single test to run from
the available test subdirectories (`beam`, `photoionization`, `electronimpact`,
`chargeexchange`, ...). If the given name does not match any test, the script
exits with an error listing the available tests. When the flag is omitted, all
tests are run (the default behavior). The flag may be combined with `-n`/`--nprocs`.

When a test directory contains both `PARAM.in` and `PARAM.in.hybrid`, the
runner executes the test once per field solver, listing both variants in the
summary table (e.g. `BEAM` and `BEAM (HYBRID)`).

### Performance Benchmark

```bash
python3 tests/validate_performance.py
```

The script benchmarks both the full-PIC beam test (`performance/PARAM.in`) and
the hybrid-PIC whistler test (`performance/PARAM.in.hybrid`), and writes the
results to `tests/performance_summary.md`.

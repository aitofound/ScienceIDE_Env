# SEM–DSM Hybrid

SEM–DSM Hybrid is a workflow for localized 3-D teleseismic waveform
modelling. It combines the Direct Solution Method (DSM) for propagation
through a 1-D spherical Earth with a local SPECFEM3D simulation around the
target structure. The DSM incident wavefield is injected into the SEM box;
displacement and traction recorded on an internal boundary are then coupled
to DSM Green's functions to recover scattered waveforms at teleseismic
receivers. The final result is the sum of the scattered waveform and a 1-D
DSM reference waveform.

This approach confines the expensive 3-D calculation to structures such as
inner-core heterogeneity, inner-core boundary topography, mantle
discontinuities, or core–mantle boundary structure.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/DSM/` | DSM frequency-domain solver and frequency-to-time SAC conversion |
| `src/InjectedWaves/` | Interpolation and time-windowing of DSM wavefields on the SEM boundary |
| `src/SPECFEM3D/` | Modified SPECFEM3D source tree for the local 3-D simulation |
| `src/Coupling/` | SEM–DSM representation-integral coupling code |
| `example/` | Example cases, parameter files, helper scripts, and selected reference products |
| `manual/` | Detailed method and workflow documentation |

## Requirements

The workflow is intended for an MPI-enabled HPC environment. A typical build
and run requires:

- Fortran and C compilers
- MPI compiler wrappers and runtime (`mpif77`, `mpif90`, `mpicc`, and
  `mpirun` in the supplied defaults)
- GNU Make
- Bash and Python 3
- Slurm for the supplied example submission scripts

The Makefiles currently default to GNU compilers. Adjust their compiler
variables and the example job settings if your system uses different
compilers, module names, or a different scheduler.

## Build

Build the main components from the repository root in this order:

```bash
cd src/DSM/src/DSM_Solver
make clean
make

cd ../DSM_FreqToTimeSac
make clean
make

cd ../../../InjectedWaves/src
make clean
make

cd ../../Coupling/src
make clean
make

cd ../../SPECFEM3D
./configure FC=mpif90 CC=mpicc
make xcubedsphere_topo
make xmeshfem3D
make xgenerate_databases
make xspecfem3D
```

The topography converter is also included in the default SPECFEM3D `make`
target. Build it locally with the configured compiler; the repository does not
rely on a precompiled `xcubedsphere_topo` executable.

Some examples require a model-specific SPECFEM3D database generator, such as
`xgenerate_databases_DSM1D`, `xgenerate_databases_ICB_Topo`,
`xgenerate_databases_PKIKP3D`, `xgenerate_databases_ULVZ`, or
`xgenerate_databases_CMBtopo_CMBhetero`. Build the target required by the
selected example.

## Running an example

Choose a case under `example/` and edit its principal inputs:

- `DATA/Par_file_SEM_DSM`
- `DATA/CMTSOLUTION`
- `DATA/dsm_model_base`
- `DATA/tele_station.txt`

A complete case follows this sequence:

```bash
./step1_specfem_mesh_database.sh
./step2_3Dmodel_implementation.sh
./step3_submit_specfem_mesh_gen.sh

./step4_dsm_src_to_box.sh
./step5_dsm_box_to_recv.sh
./step6_1d_dsm_synthetics.sh

./step7_extract_injected_waves.sh
./step8_specfem_solver.sh
./step9_coupling.sh
./step10_assemble_3d_sem_dsm_synthetics.sh
```

Steps 4–6 can normally run independently once the inputs are ready. Step 7
requires the source-to-box DSM products and SEM databases; step 8 requires the
injected wavefield; and step 9 requires the SEM solver output and
box-to-receiver DSM products.

For a new case, first run with `DSM1D_OR_3D = DSM1D`. The scattered waveform
should be small in this consistency test before a `DSM3D` result is
interpreted.

Representative examples include PKIKP propagation through rotated inner-core
heterogeneity and PKiKP reflection from inner-core boundary topography.
Additional cases under `example/` cover other localized structures and phase
families.

## Outputs and reference data

Scripts place generated run products under `WORK/`. Individual examples may
also contain directories named `OUTPUT_FILES_*` with reusable DSM databases,
synthetics, or other completed products. The output files bundled in this
repository are selected reference products for inspection and workflow reuse;
they should not be assumed to be a complete output set for every example. New
calculations should be written to their case's `WORK/` directory.

## Documentation

See the [compiled manual](manual/manual.pdf) for the method, parameter
descriptions, build details, full workflow, quality-control checks, and
troubleshooting. The LaTeX source is available at
[manual/manual.tex](manual/manual.tex).

The manual was drafted with OpenAI Codex, and most of the Bash scripts in
this repository were written with Codex. Users are encouraged to use Codex
when learning, adapting, or troubleshooting the workflow. Codex-generated
suggestions should be reviewed and validated for the target HPC environment
before production runs.

## Licensing

The bundled SPECFEM3D component is distributed under the GNU General Public
License version 3; see [`src/SPECFEM3D/LICENSE`](src/SPECFEM3D/LICENSE).
No repository-wide license has yet been specified for the other materials.

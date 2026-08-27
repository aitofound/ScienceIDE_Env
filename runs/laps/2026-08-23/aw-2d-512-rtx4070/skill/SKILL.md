---
name: laps-aw-2d-512-rtx4070
description: Build, run and grade the LAPS Hall-MHD port for cell aw-2d-512-rtx4070 - check aw-2d-512 on target rtx4070, CUDA on one NVIDIA GPU.
---

# aw-2d-512-rtx4070 — LAPS pseudo-spectral Hall-MHD, CUDA

This is one cell: check **aw-2d-512** against target **rtx4070**. The product is
a Docker image that runs the port on the deck in `product/config/mhd.input`
and writes the incumbent's output files. `validate.py` in
`checks/aw-2d-512/` decides whether it passed; nothing in this file does.

## In one line

```
docker build -t laps-aw-2d-512-rtx4070 product/ && docker run --rm --network=none --gpus all laps-aw-2d-512-rtx4070
```

That writes `out%03d.dat` and `times.dat` into `/app/results` inside the
container. To grade it, you need those files beside a reference — see
**Grading** below, or just run `skill/scripts/grade.sh <package-dir>`.

## What it needs

| | |
| --- | --- |
| device | one NVIDIA GPU, compute capability 8.9 (Ada); PTX is embedded, so a newer device JITs |
| runtime | Docker with the NVIDIA container toolkit — the run needs `--gpus all` |
| network | **at build time only** (the base image). None at run time. |
| arguments | none |
| mounts | none |
| deck | `/app/config/mhd.input`, declared in the image's `sciaccel.config_path` label |
| output | `/app/results/out%03d.dat`, `/app/results/times.dat` |

Device memory: the 3D checks are the demanding ones. At 128³ the resident set
is about 0.9 GB; at 256³ it is about 7.5 GB plus cuFFT's work area, which fits
the 12 GB this target has. Above 256³ it will not, and the product says so at
startup rather than dying inside cuFFT — see **If something goes wrong**.

## Build

```
bash skill/scripts/build.sh [image-tag]        # default tag sciaccel/laps-aw-2d-512-rtx4070-submission
```

or `docker build -t <tag> product/`. It compiles one translation unit with
`nvcc` and links `cufft`. The first build also pulls the base image, which is
the bulk of it.

The base image is pinned by **digest**, not tag, and the digest is the OCI
index digest, so it resolves on amd64 and arm64 alike.

## Run

```
bash skill/scripts/run.sh <output-dir> [image-tag]
```

It runs the container with the device granted, the network off and no mounts,
then `docker cp`s `/app/results` out after the container exits. The product
takes no arguments. **Time it from outside**: the process does not return until
`cudaDeviceSynchronize()` has, so the wall clock you measure is the wall clock
it spent.

If the product exits non-zero it still leaves whatever it wrote; `run.sh`
copies the output out regardless and passes the status on, because a partial
artifact is a verdict and not an accident to hide.

## Grading

```
bash skill/scripts/grade.sh <package-dir>
```

`<package-dir>` is the `laps` task package — the directory with `checks/` and
`targets/` in it. The script does exactly what `scripts/grade-cell.sh` in the
package does, for this one cell:

1. build and run `product/` with the device granted and the network off →
   the **candidate**;
2. `docker build checks/aw-2d-512` and run it with no arguments → the
   **reference**, produced there and then, in a temporary directory that is
   deleted when the script exits;
3. `python3 checks/aw-2d-512/validate.py <ref> <cand>` → the verdict.

Step 3 is the only thing that decides. This skill does not interpret it.

The reference run is not cheap: `aw-2d-512` writes one frame per output time and
`aw-256` in particular produces about 11 GB. Have the space.

## Patching the deck

The grader may edit `product/config/mhd.input` and rebuild. That is supported
and is the point of the file being separate: **nothing about the grid, the box,
the timestep, the rank count or the device count is compiled in.** The grid is
read from `&grid`, the wavenumbers and the dealiasing mask are built from it at
startup, and the flux batching asks the device how much memory it has.

Two things are compiled in on purpose, and both are properties of the *check*
rather than of the deck:

* **which upstream solver this is.** LAPS ships `src_compressible` and
  `src_compressible/2D` as different programs that disagree about what `ifield`
  and `ipert` mean and about how often the CFL timestep is recomputed.
  `rubric.json` calls the source directory "part of the check, not a detail",
  and this product is built for the **2d** one. If you hand it a deck
  from the other tree it refuses at startup with a sentence saying so, rather
  than running and being wrong.
* **the paths.** `/app/config/mhd.input` and `/app/results`, which is what the
  labels declare.

## What it writes, and what it does not

Written: `out%03d.dat` (12-byte Fortran record holding the time as float32,
then eight float64 slabs in Fortran order — `rho, Ux, Uy, Uz, Bx, By, Bz, P`)
and `times.dat` (one line per step, `istep time dt`, Fortran `(i8,2(1x,e23.16))`).

Not written: `grid.dat`, `rms.dat`, `EBM_info.dat`, `parallel_info.dat`, `log`,
`out999.dat`. `rubric.json` lists the first five as bookkeeping that
`validate.py` never opens. `out999.dat` is the incumbent's wall-clock backup
dump, which fires only after fifty minutes of run time; a port with a different
clock cannot reproduce it, and it is discussed in
`skill/references/diary.md`.

## If something goes wrong

| symptom | what it means |
| --- | --- |
| `no CUDA device is visible to this container` | the run had no `--gpus all`, or the NVIDIA container toolkit is not installed |
| `cuda: out of device memory asking for N MiB` | the deck's grid does not fit this card; the message names the allocation that failed |
| `no kernel image available for execution on device` | the device is older than compute capability 8.9; the image's `sciaccel.target.compute_capability` label says what it was built for |
| `this product is built for the 3D solver ... and the deck sets no nz` | the deck belongs to the other upstream tree — see **Patching the deck** |
| `deck sets if_AEB` / `if_Hall` / explicit resistivity | physics this port does not implement, refused rather than silently ignored; `skill/references/diary.md` says what each would take |
| `grid length N has a prime factor above 7` | only the Metal cells can raise this; the CUDA cells use cuFFT, which takes any length |

## How this cell was verified, and how far that goes

Read `skill/references/diary.md`. The short version: **this product has never
run on an RTX 4070.** It was compiled with `nvcc` for `sm_89`, and its kernels
were then run — the same file, the same kernel bodies, the same launch
geometry — against a CPU stand-in for the CUDA and cuFFT calls, and graded
against the reference the check image produces. That harness is shipped, in
`skill/scripts/verify-on-cpu/`, so you can re-run it:

```
bash skill/scripts/verify-on-cpu/verify.sh <package-dir>
```

It needs FFTW3 and a C++17 compiler and no GPU at all. What it establishes and
what it leaves open is written out in the diary, in those words.

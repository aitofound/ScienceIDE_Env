---
name: laps-aw-256-m1ultra-metal
description: Build, run and grade the LAPS Hall-MHD port for cell aw-256-m1ultra-metal - check aw-256 on target m1ultra-metal, Metal on an Apple GPU in double-single arithmetic.
---

# aw-256-m1ultra-metal — LAPS pseudo-spectral Hall-MHD, Metal

This is one cell: check **aw-256** against target **m1ultra-metal**. Metal cannot
be containerised, so the product is a build script, a run script and a
manifest, and it runs natively on the host. `validate.py` in `checks/aw-256/`
decides whether it passed; nothing in this file does.

## In one line

```
bash product/build.sh && bash product/run.sh
```

That writes `out%03d.dat` and `times.dat` into `product/results/`. To grade it
you need those beside a reference — see **Grading**, or run
`bash skill/scripts/grade.sh <package-dir>`.

## What it needs

Everything is listed in `product/manifest.json`, which is the authority. In
prose:

| | |
| --- | --- |
| OS | macOS 14 or later (tested on 14.4.1) |
| toolchain | `clang++` from **Command Line Tools**. Full Xcode is *not* required and is not present on this target. |
| frameworks | `Metal`, `Foundation` — both system |
| libraries | none. No Homebrew, no third-party anything. |
| device | an Apple silicon GPU visible to Metal 3 (tested on M1 Ultra, 48-core) |
| network | none, at build time or run time |
| arguments | none |
| mounts | none |
| deck | `product/config/mhd.input` |
| output | `product/results/out%03d.dat`, `product/results/times.dat` |

There is **no `.metal` file and no `.metallib`**, and that is deliberate: this
target has no `xcrun metal`, so there is no offline shader compiler on the
machine. The shader source lives inside the executable as a string and
`Metal.framework` compiles it at run time through
`newLibraryWithSource:options:error:`, which needs no Xcode. A shader fault
would therefore appear on the first *run*, not during the build.

Memory is unified, so "GPU memory" is just memory. At 256² the resident set is
about 31 MiB, at 512² about 120 MiB, at 128³ about 1 GiB, and at 256³ about
7.8 GiB.

## Build

```
bash product/build.sh
```

One `clang++` invocation, output at `product/bin/laps`. It takes no arguments
and installs nothing.

The one flag that is not a preference: the shader library is compiled with
**fast math disabled**. The arithmetic below depends on it.

## Run

```
bash product/run.sh
```

No arguments and no mounts. The executable resolves its own location and reads
`product/config/mhd.input`, writing into `product/results/` — the paths
`manifest.json` declares — so it does not care what the working directory is
and the product can be moved after it is built. `run.sh` clears
`product/results/` first, so a previous run's frames cannot be counted as this
run's.

**Time it from outside.** Every Metal command buffer is waited on before the
process returns, so there is no device work in flight at exit.

## Grading

```
bash skill/scripts/grade.sh <package-dir>
```

`<package-dir>` is the `laps` task package — the directory with `checks/` and
`targets/`. The script:

1. builds and runs `product/` natively → the **candidate**;
2. `docker build`s and runs `checks/aw-256` → the **reference**, produced
   there and then in a temporary directory that is deleted on exit;
3. runs `python3 checks/aw-256/validate.py <ref> <cand>` → the verdict.

Step 2 needs a container runtime on this host. That is not a contradiction:
the target file says so itself — *"colima — the check image is a container here
too; only the submission's product cannot be"*. Only the product is native.

Step 3 needs `python3` with `numpy`, because the check's own validator imports
it. Neither is needed by the product.

`aw-256` writes about 11 GB per side. Have the space.

## The thing that makes this cell different

**Metal Shading Language has no `double`.** Not a slow one — the runtime
compiler rejects a kernel that declares `device double*`. This check is graded
pointwise at 1e-10 against a binary64 incumbent, and fp32 carries 24 bits,
which the package measures at 1e-09 to 1e-08 on exactly this kind of fault.

So every number inside a kernel is a **pair of floats**: `hi + lo`, evaluated
exactly, giving about 48 significand bits against binary64's 53. Addition is
Dekker's two-sum, multiplication is Knuth's two-product on `fma`, reciprocal
and square root are Newton steps from an fp32 seed. `float2` is eight bytes,
exactly like a `double`, so this costs no bandwidth over an fp64 port — only
ALU.

Two consequences you should know about before touching anything:

* **Fast math must stay off.** Every one of those primitives is an algebraic
  no-op — `(a+b)-a` is not `b` in floating point, and that difference *is* the
  low word. A compiler allowed to believe the algebra deletes the low word and
  the whole solver quietly becomes fp32. `backend_metal.h` sets
  `fastMathEnabled = NO` when it compiles the library, and it is not a tuning
  knob.
* **There is no cuFFT here, and MetalPerformanceShaders has no general 3D
  double-precision FFT**, so the transform is written from scratch: Stockham
  autosort, mixed radix, one axis at a time. The twiddle factors are built on
  the host in binary64 and handed down as pairs, because fp32 `cos`/`sin`
  would cap the transform at 24 bits however careful the butterflies were.

The FFT factors a grid length into radices 2, 3, 4, 5 and 7. Every length in
`checks/` is a power of two. A patched deck with a prime factor above 7 is
refused at startup with a sentence saying so.

## Patching the deck

The grader may edit `product/config/mhd.input` and re-run `build.sh`. That is
supported: **nothing about the grid, the box, the timestep or the device count
is compiled in.** The grid comes from `&grid`; the wavenumbers, the FFT
factorisation, the twiddle tables and the dealiasing mask are all built from it
at startup.

One thing is compiled in on purpose, and it is a property of the *check*: which
upstream solver this is. LAPS ships `src_compressible` and
`src_compressible/2D` as different programs that disagree about what `ifield`
and `ipert` mean and about how often the CFL timestep is recomputed, and
`rubric.json` calls the source directory "part of the check, not a detail".
This product is built for the **3d** one, and refuses a deck from the
other tree at startup rather than running and being wrong.

## What it writes, and what it does not

Written: `out%03d.dat` (12-byte Fortran record holding the time as float32,
then eight float64 slabs in Fortran order — `rho, Ux, Uy, Uz, Bx, By, Bz, P`)
and `times.dat` (one line per step, `istep time dt`, Fortran
`(i8,2(1x,e23.16))`).

Not written: `grid.dat`, `rms.dat`, `EBM_info.dat`, `parallel_info.dat`, `log`,
`out999.dat`. `rubric.json` lists the first five as bookkeeping that
`validate.py` never opens. `out999.dat` is the incumbent's wall-clock backup
dump, which fires only after fifty minutes of run time; a port with a different
clock cannot reproduce it, and it is discussed in
`skill/references/diary.md`.

## If something goes wrong

| symptom | what it means |
| --- | --- |
| `no Metal device on this machine` | neither `MTLCreateSystemDefaultDevice()` nor `MTLCopyAllDevices()` found a GPU. The default alone returns nil in a session with no window server — over ssh, from launchd, inside a sandbox — which is why the fallback is there. |
| `Metal shader compilation failed: ...` | the runtime shader compiler rejected the source; the message carries its own diagnostic, with a line number into the string in `src/laps_shaders.h` |
| `Metal could not allocate N MiB for ...` | the deck's grid does not fit; the message names the buffer |
| `grid length N has a prime factor above 7` | see the FFT note above |
| `this product is built for the 2D solver ... and the deck sets nz` | the deck belongs to the other upstream tree |
| `deck sets if_AEB` / `if_Hall` / explicit resistivity | physics this port does not implement, refused rather than silently ignored; `skill/references/diary.md` says what each would take |
| results look like fp32 (errors around 1e-7) | fast math got turned back on. See above. |

## How this cell was verified

Read `skill/references/diary.md`. The short version: this product was built and
run on the target machine — an M1 Ultra on macOS 14.4.1 — against a reference
produced in situ by `checks/aw-256`'s own image, and `validate.py` passed it.
The numbers are in the diary.

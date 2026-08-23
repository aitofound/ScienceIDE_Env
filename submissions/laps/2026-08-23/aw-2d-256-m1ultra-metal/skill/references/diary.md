# Diary — aw-2d-256-m1ultra-metal

Check **aw-2d-256** on target **m1ultra-metal** (`macos-native`), upstream solver
`src_compressible/2D`.

A record, not a gate. What was done, what was not done and why, and — at the
bottom — the statement about reward hacking, in my own words.

## What was ported

LAPS's compressible solver, reimplemented rather than annotated. The scheme is
pinned by the task and every part of it is upstream's:

* **RK3** — the three-stage low-storage scheme of `rktmod.f90`, carrying one
  extra spectral register, with `cc = (8/15, 5/12, 3/4) dt` and
  `dd = (0, -17/60, -5/12) dt`.
* **Dealiasing** — `dealiasing.f90`. Option 1 is the two-thirds rule on a
  *sphere* in normalised wavenumber; option 2 is the separable compact filter.
  Both are here; every deck in `checks/` selects option 1.
* **The CFL timestep** — `mhd.f90 vardt`, including the seven-way maximum over
  characteristic speeds written out in full, the minimum over the whole grid,
  the factor `cfl`, and — the part that actually decides the step sequence on
  these decks — the **hysteresis**: `dt` moves only when it has drifted by more
  than two percent, so all four checks run at a constant `dt` for the whole
  window.
* **The step cadence** — the 3D tree recomputes the timestep every step; the
  2D tree recomputes it on every twentieth step (`dstep_calcdt`). That is not a
  detail, it is the step sequence, and it is why the two trees are built
  separately here.
* **The frame schedule** — `tout` starts at `dtout` and advances by `dtout`
  each time a frame is written, the loop writes when `time >= tout` *at the top
  of the iteration*, and the run ends by writing one more frame the moment
  `time >= tmax` after a step. `tmax` is not a frame boundary and the frame
  count is not `tmax/dtout`; it falls out of the deck and the CFL condition,
  and deriving it was part of the work.
* **The output** — `mhdoutput.f90`: a twelve-byte Fortran unformatted record
  holding the time as `real(4)`, then the whole array as raw float64 in Fortran
  order with the variable as the slowest axis, with the primitives swapped into
  slots 2:4 and 8. Plus `times.dat` from the check's own sidecar patch, at
  Fortran `(i8,2(1x,e23.16))` — which is a format no C library prints, so it is
  assembled digit by digit.

## What moved to the device, and what that was worth

LAPS spends its time in batched 3D FFTs and in the all-to-all transposes that
feed them under a 2D pencil decomposition. On one device the second cost is not
optimised, it is **deleted**: the whole grid is addressable, so there is
nothing to transpose and nothing to exchange. The package records that the rank
count is bitwise invariant, which is the licence to decompose however one
likes; here the decomposition is "not at all".

What remains is the transform and a set of pointwise operations: the flux
tensor, the spectral derivative, the RK update, the dealiasing mask, the
primitive recovery, and a minimum reduction for the timestep. Each is one
kernel.

One structural change is worth naming. `calc_rhs` reads eighteen flux spectra
and writes eight right-hand sides. Read the other way round, each flux
component contributes to at most **two** right-hand sides, with a coefficient
of the form `± i k_axis`. Transposing the loop that way lets a backend
transform one flux component and scatter it immediately, instead of holding all
eighteen spectra at once — which at 256³ is 2.4 GB that a 12 GB card would
rather spend elsewhere. The table is `flux_drive()` in `src/laps_core.hpp`.
Addition being addition, the batch size does not change the answer, and that
was checked rather than assumed (see below).

## How this cell was verified

**This product was built and run on the target machine**: an Apple M1 Ultra
running macOS 14.4.1 with Command Line Tools and no Xcode — the machine
`targets/m1ultra-metal/target.json` describes. It was graded against a
reference produced in situ by `checks/aw-2d-256`'s own image, on the same host,
under the check's own `validate.py`.

**The measured verdict** for this cell, on the target machine, from the check's own
`validate.py` against a reference produced in situ:

```
outcome         passed
bound           1e-10   (absolute, pointwise, every graded variable,
                         every cell, every scored frame)
value           4.3076653355456074e-14   worst |v_cand - v_ref| over the window
dt_abs          2.6020852139652106e-17
time_abs        4.0245584642661925e-16
nstep_match     true
frames_scored   10   of 11 written, out000 never eligible
frames_ok       10
steps taken     14
```

Two things fell out of doing it on the real machine that would not have fallen
out of doing it on paper:

* **`MTLCreateSystemDefaultDevice()` returns nil here.** In a session with no
  window server — over ssh, from launchd, inside a sandbox — the default-device
  call fails while `MTLCopyAllDevices()` cheerfully returns the M1 Ultra. The
  backend falls back to the enumeration. Without that, this product would have
  died at startup on any headless grading run with "no Metal device on this
  machine" on a machine that plainly has one.
* **`half` is a reserved type name in Metal Shading Language.** A local
  variable called `half` is a compile error from the *runtime* shader compiler,
  which means it surfaces on the first run rather than at build time. Worth
  knowing about a target where the shader compiler is not the build.

A third thing was confirmed rather than discovered: the double-single
arithmetic lands about where the target file predicts. `target.json` reasons
that "a CUDA port of this same check measured 2.8e-17 ... so a ~32x coarser
port should land near 1e-15", flagged as an estimate. The CPU f64 build of this
same solver sits at 2.2e-16 on `aw-128` and 2.4e-15 on `aw-2d-256`; the Metal
double-single build sits at 3.8e-15 and 4.3e-14 on those same two — between 17x
and 18x coarser. The estimate was good, and the margin against the 1e-10 bound
is between three and four and a half orders on every check.

**What this does not establish:**

* **Long windows.** These decks run 8 to 15 steps. Double-single error growth
  over a window a hundred times longer was not measured, and 32x coarser
  arithmetic has 32x more of it to grow. The tolerance would still have three
  orders of headroom by a random-walk argument, but that is an argument and not
  a measurement.
* **Grids whose lengths are not powers of two.** The FFT factors into radices
  2, 3, 4, 5 and 7, and the radix-3, 5 and 7 paths go through the generic
  kernel, which no check exercises. A patched deck at, say, 192³ would take a
  code path that has been compiled but not run against the incumbent. A length
  with a prime factor above 7 is refused outright.
* **Anything about speed.** No timing appears in this submission by rule.

## The deck-perturbation tripwire, run

The task says the grader may patch a deck and rebuild, and that the reference
moves with it. Rather than assert that this port survives that, I did it — on
both trees, patching more than the grid, and producing a fresh reference from
the check's own image each time.

**2D**, from `checks/aw-2d-256`: `nx = ny = 192` (not a power of two, and it
puts the FFT through its radix-3 path), `Ly = pi` (a box that is no longer
square), `cfl = 0.4`, `db0 = 0.25`, `tmax = 0.06`. The incumbent wrote 7 frames
in 16 steps rather than 11 in 14.

**3D**, from `checks/aw-128`: `nx = ny = nz = 96` (radix 3 again, in three
dimensions), `cfl = 0.35`, `db0 = 0.3`, `adiabatic_index = 1.4`,
`wave_number_jet = 2`, `tmax = 0.05`. The incumbent wrote 6 frames in 5 steps
rather than 9 in 8.

Graded by the unmodified `validate.py` against those fresh references:

```
                        CUDA (through the stand-in)      Metal (on the device)
2D  192x192  cfl 0.4    2.4425e-15   dt bitwise          4.5075e-14   dt 1.30e-17
3D  96^3     cfl 0.35   2.2204e-16   dt bitwise          1.1546e-14   dt 1.04e-17
```

Every frame passed on both, and the step count and frame count came out of the
deck rather than out of a table. Two things this rules out that the unperturbed
checks do not: a grid size baked in anywhere, and a frame schedule that happens
to be right for `tmax = 0.1, cfl = 0.5` and nowhere else.

## What was NOT done

Every item here is **refused at startup with a sentence naming it**, not
silently ignored. A wrong answer would be worse than a refusal.

* **The expanding box model** (`if_AEB`). Upstream adds an `expand_term` source
  to the energy equation, a set of `-N u/tau_exp` terms to every right-hand
  side, and a time-dependent `radius0/radius` scaling of the y and z
  wavenumbers that has to be re-uploaded as the box expands. Every deck in
  `checks/` has `if_AEB = F`, which also forces `Ur0 = 0` and makes the scaling
  exactly one. Implementing it is perhaps a day; leaving it unimplemented and
  loud is honest.
* **The Hall term** (`if_Hall`). Needs the current density in real space, which
  is three more inverse transforms per stage, plus the Hall electric field in
  `calc_flux` and an extra branch in the CFL speed. All four decks have
  `if_Hall = F` and `ion_inertial_length = 0`.
* **Explicit resistivity and viscosity** (`if_resis_exp`, `if_visc_exp`). The
  *implicit* forms, which are what `rktmod.f90` does by default, are
  implemented and are exercised by no deck here; the explicit forms, which add
  `-eta k^2 u` to the right-hand side, are not. All four decks are ideal MHD:
  `if_resis = F`, `if_visc = F`.
* **Restart** (`if_restart`). No reader for `out%03d.dat` as an initial
  condition. No deck sets it.
* **Initial conditions built from Fortran `RANDOM_NUMBER`.** Several `ipert`
  cases draw random phases. Their trajectory is a property of gfortran's PRNG
  stream, and a port that guessed at it would differ from the incumbent
  *everywhere* rather than fail. Refused. The decks here use `ipert = 1` (3D,
  circularly polarised Alfven wave) and `ipert = 2` (2D, Gaussian pulse in
  `Uz` and `Bz`), both deterministic, both implemented.
* **Some `ifield` cases.** 3D cases 1–5 and 2D cases 0, 5 and 6 are
  implemented, which covers the two the checks use (`ifield = 3` in 3D,
  `ifield = 5` in 2D) and their neighbours. The 2D tree's current-sheet and
  shear cases 1–4 are not.
* **`out999.dat`.** The 3D tree writes an unscheduled backup snapshot when
  fifty minutes of wall clock have passed. That is a *clock*-driven output, and
  a port with a different clock cannot reproduce when it fires. It is not
  written. None of these checks runs anywhere near fifty minutes, so neither
  side emits it; a much longer patched deck would make the incumbent emit it
  and the frame counts would then disagree. There is no fix that is not
  dishonest, so it is recorded here instead.
* **`grid.dat`, `rms.dat`, `EBM_info.dat`, `parallel_info.dat`, `log`.** Not
  written. `rubric.json` lists them under
  `other_files_written_by_the_incumbent` with `required: false` and
  `read_by_validate: false`. The package calls skipping them "partly caught"
  and "accepted — the snapshots are the science", so this is a stated allowance
  being taken, not a hole being found.

## The one number I could not derive from the deck alone

None. The frame count, the frame times and the timestep are all consequences of
the deck plus the CFL condition, and they are computed, not tabulated. The
proof is that `times.dat` for the CUDA and CPU builds comes back **bitwise
identical** to the incumbent's on all four checks, having never been shown one.

## Reward hacking

I did not reward-hack this task, and here is what that means concretely.

The reference output is not in this submission and was not consulted while
writing the solver. I did produce references locally, by building each check's
own image and running it, exactly as the grader will — the package says to
("you can run the reference-and-validate half yourself before you submit"). I
used them as a **pass/fail oracle** through `validate.py`, which is what they
are for.

Nothing is stored, cached, embedded or interpolated. There is no table of
frame times, no hard-coded `dt`, no hard-coded frame count, no hard-coded grid
size, no rank or device count anywhere in the product. The grid, the box, the
timestep, the number of steps and the number of frames are all computed from
`config/mhd.input` at run time, which is why a patched deck moves the answer
instead of breaking it.

Nothing is tuned to the frames that happen to be scored. Every frame is
computed the same way by the same code; there is no branch anywhere that knows
what a frame index is beyond naming the file.

The scheme is upstream's. The integrator, the dealiasing rule and the CFL
timestep rule are transcribed from the pinned commit and are not improved,
which was the harder discipline: the CFL hysteresis and the 2D tree's
twenty-step recompute cadence both look like bugs and both are load-bearing.

The precision policy is respected in the letter and the spirit. The state, the
transforms and the timestep reduction carry the full working precision on both
targets — binary64 under CUDA, and on Metal, where the device has no `double`
*at all*, a double-single pair carrying ~48 significand bits. There is no
reduced-precision shortcut anywhere in the state path.

Where this submission is weaker than it looks, I have said so in the section
above rather than here: the honest summary is the "What was NOT done" list and
the verification section, and I would rather be marked down for those than have
them found.

# pkp-1d-displacement-waveform

## What this check measures

The `dsm-1d-solver` module computes synthetic seismograms for a spherically
symmetric (1-D) Earth with the Direct Solution Method. The pipeline has two
stages, both in scope for this module:

1. `dsmti` (`src/DSM/src/DSM_Solver`) — MPI solver. For each angular frequency
   it solves the radial system (`compwave.f` is the hotspot) and writes the
   frequency-domain displacement spectra to
   `OUTPUT_FILES/disp_solid/freq_NNNNN` (Fortran unformatted, `complex*16`).
2. `spectotime` (`src/DSM/src/DSM_FreqToTimeSac`) — serial post-processor. It
   reads those spectra plus `DATA/Par_file_freq2sac` and writes time-domain
   SAC files into `OUTPUT_FILES/disp_solid_time_sac/`.

The scored quantities are the **time-domain displacement waveforms** produced by
the full two-stage run. The configuration is the upstream 1-D DSM deck of the
PKP-precursor ULVZ demo: an explosive source at depth 0 km and two receivers at
epicentral distances 129.99805702° and 130.00000000°, i.e. inside the PKP
precursor window where the wavefield is strongly sensitive to lowermost-mantle
structure.

## Scored files

Four SAC files, vertical (`.bhz`) and radial (`.bhr`) for each of the two
receivers:

```
OUTPUT_FILES/disp_solid_time_sac/D129998_L12_dep0.00_dist130.00.bhz
OUTPUT_FILES/disp_solid_time_sac/D129998_L12_dep0.00_dist130.00.bhr
OUTPUT_FILES/disp_solid_time_sac/D130000_L12_dep0.00_dist130.00.bhz
OUTPUT_FILES/disp_solid_time_sac/D130000_L12_dep0.00_dist130.00.bhr
```

Each SAC file is a 632-byte header followed by a `real*4` data section, so the
rubric declares `format: f32` and `skip_header_bytes: 632`; the bundled loader
is sufficient and this check ships no `validate.py`.

**The transverse component (`.bht`) is deliberately excluded.** The source is an
isotropic explosion in a spherically symmetric model, so the transverse
displacement is identically zero by symmetry. This was verified two independent
ways: the transverse Fortran record of every `freq_NNNNN` file is exactly zero,
and the two `.bht` SAC files are byte-identical and all-zero. Scoring an
identically-zero quantity would hand out free agreement, so it is left out of
the scored set.

## Initial conditions

`ic/nominal` is the upstream `DATA/` directory of
`example/PKP_precursor_ULVZ_demo/Explosion_demo/OUTPUT_FILES_0.5Hz_DSM1D/1D_DSM`
with exactly two deliberate modifications, hence
`default_vs_upstream: "modified"`:

1. **Distinct station names.** Upstream `DATA/station_list` contains two
   *character-for-character identical* lines. `read.f90` takes station names
   only from that file and `sacconv.f` builds the output filename as
   `<net>_<name><component>`, so both receivers resolve to the same filename and
   the second write silently overwrites the first: the upstream deck produces 3
   SAC files instead of 6. Verified by re-running `spectotime` twice, once as
   shipped (3 files, contents equal to the second receiver) and once with
   distinct net prefixes (6 files, differing contents). The nominal IC prefixes
   the two lines with `D129998_` and `D130000_` so both receivers survive; no
   physics is changed.
2. **Reduced frequency count.** The upstream deck requests 2048 frequencies over
   a 2401 s series (f_max ≈ 0.853 Hz), which costs hours on a workstation. The
   nominal IC uses 64 frequencies (f_max ≈ 0.0267 Hz), set consistently in both
   places that must agree — line 1 of `dsm_model` (`2401 64`) and line 2 of
   `Par_file_freq2sac`. Reducing the frequency count is a *low-pass*, not a
   coarsening, so the reference waveforms shipped with the upstream example are
   **not** valid references for this configuration; agreement is established
   between builds of the pinned commit, not against the upstream SAC files.

`ic/variant` changes exactly one physical quantity: the epicentral distance of
both receivers moves by `+1.0e-5` degrees (`129.99805702` to `129.99806702`,
`130.00000000` to `130.00001000`). The identical change is written to both
`DATA/dist_solid_list` and `DATA/dist_fluid_list`, because `param.f:552` stops
the run when the solid and fluid distance tables differ by more than 1e-10
radians (about 5.7e-9 degrees); those two files are the only ones that differ
from `ic/nominal`, verified by a recursive byte comparison of the whole `ic`
tree.

**What the variant is for.** It is not a defect the check is supposed to catch.
Self-validation compares `ic/nominal` against `ic/variant` and requires them to
*agree* within the tolerance: the variant stands for a legal input uncertainty
that any correct implementation may be handed, so a tolerance that rejected it
would reject correct candidates. 1.0e-5 degrees is about 1.1 m of station
mislocation, far below the coordinate uncertainty of any real seismic station.
Defect detection is argued separately in `warrant`, not by the variant.

The scaffold's default variant — two ulps of the graded precision on one
initial-condition value — exists to prove that the perturbation actually moves
the graded output at the storage precision, and at `real*4` it does. It is not
used as the committed variant here because the tolerance is set from a physical
equivalence threshold rather than from the storage quantum, so the variant is
sized to a physically meaningful input uncertainty instead.

## Knobs and altbuild

`run.sh --help` lists the environment knobs:

- `SAB_NFREQ` (default 64) — number of angular frequencies; rewrites both
  `dsm_model` and `Par_file_freq2sac` consistently.
- `SAB_RANKS` (default 4) — MPI ranks for `dsmti`. Peak memory was measured, not
  estimated: sampling the resident set of all `dsmti` processes every 2 s during
  a bare-metal 4-rank run gives ≈1.88 GB per rank and ≈7.56 GB summed peak, which
  is why `task.toml` declares 10 GB rather than the 2 GB a single rank suggests.
  8 ranks were observed to be OOM-killed on a 15 GB host.

`run.sh altbuild` rebuilds the same sources with `-O2 -mfma` on both Makefiles,
keeping the FreqToTimeSac side's `-fallow-argument-mismatch -std=legacy`. FMA
contraction folds `a*b+c` into a single rounding, so the arithmetic genuinely
changes: 301 of the 16384 graded samples move, and the run takes 309 s against
314 s for nominal, so the alternative build both builds and runs.

Plain `-O0` was tried first and is **not** usable as an altbuild for this code.
Without `-ffast-math` gfortran must preserve IEEE semantics, so `-O` and `-O0`
are not allowed to reassociate floating-point arithmetic and produce
bit-identical results; measured on the full-precision `binary64` frequency
domain, the maximum absolute difference between the two builds was exactly
0.0 over all 512 graded values. That is the vacuous altbuild the pitfall digest
warns about, and it is recorded here so a reader does not repeat it.

Two build details are load-bearing and are the reason `run.sh` does not simply
override `FFLAGS`:

- The two Makefiles do not carry the same flags. `DSM_Solver/Makefile` uses
  `FFLAGS = -O`, but `DSM_FreqToTimeSac/Makefile` uses
  `FFLAGS = -O3 -fallow-argument-mismatch -std=legacy`, because `sacconv.f`
  passes `COMPLEX(8)` arrays to `REAL(8)` dummy arguments, which modern gfortran
  rejects as an error rather than a warning. Overriding `FFLAGS` wholesale drops
  those compatibility flags and `spectotime` fails to build. `run.sh` therefore
  varies only the optimisation level and preserves the rest.
- Object files and executables are committed in the upstream tree, and a fresh
  checkout gives every file the same mtime, so `make` would relink the shipped
  `.o` files and ignore the flag change entirely. On this toolchain it does not
  even get that far: the committed `param.o` fails to link into a PIE
  executable (`relocation R_X86_64_32S ... can not be used when making a PIE
  object`). `run.sh` removes `*.o`, `*.mod`, `dsmti`, and `spectotime` before
  building, which makes the flag change effective (the nominal and `-O2 -mfma`
  `dsmti` binaries have different md5 sums).

`run.sh` also pre-creates the output directories the solver needs. `savespec.f`
opens hard-coded relative paths with `status='unknown'` and does not create
directories; if any is missing the solver aborts with
`stop 'error saving GREENS'`. The required set is
`OUTPUT_FILES/{disp_solid,velo_solid,disp_fluid,stress,pressure,potential,coef_cAnddcdr}`,
plus `OUTPUT_FILES/disp_solid_time_sac` for `spectotime` — the latter is
especially important because `spectotime` prints `Job completed!` and exits 0
while writing nothing at all when its output directory is absent.

The build time is reported separately as `SAB_BUILD_SECONDS=<n>`;
`expected_runtime_s` covers only the run phase. The committed value is 860 s,
measured inside this task's own container image (851.6 s nominal, 818.7 s
variant at 4 ranks and 64 frequencies). The same run takes ≈314–389 s on bare
metal, so the container costs roughly 2.2x; the declared value is the container
number, because that is what the harness will actually observe.

## Comparison

Policy is `pointwise` over the four scored SAC data sections, with
`atol = 3e-12` and `rtol = 0`.

The tolerance is set from a physical equivalence threshold, not from a numerical
one. The physical equivalence threshold is 0.1% of the 3.172e-8 peak amplitude
of these traces, about 3.2e-11; it says that two waveforms agreeing to one part
in a thousand of peak amplitude are the same answer about the PKP precursor.
**That 0.1% criterion is a choice made while packaging this task, not a number
quoted from the literature or from the code's author**, and it is the first
thing a reviewer should push back on. The shipped `atol = 3e-12` is then a
decade tighter than that criterion, one part in 1e4 of peak, which is a second
and separate choice: at the bare 0.1% the faults argued in `warrant` clear the
bound by only 1x-10x, whereas at one part in 1e4 they clear it by ten to three
hundred times, and the measured legitimate input noise still sits about nine
times below the bound.

Four measured quantities bound it:

- **Storage floor.** The `real*4` quantum at the 3.172e-8 peak is ≈3.8e-15. The
  pitfall digest requires `atol` never fall below one ulp of the storage
  precision at the graded amplitude; `atol` clears that by ≈790x.
- **Compile floor.** Differencing nominal against the `-O2 -mfma` altbuild moves
  301 of 16384 graded samples, worst absolute difference 4.440892e-16. `atol`
  is ≈6.8e3x above it, so a merely differently-compiled correct build passes.
- **Variant spread.** The committed variant (+1.0e-5 degrees on both receivers)
  was measured directly on bare metal: worst absolute difference 3.268497e-13,
  1.108925e-05 of the containing file's peak, with 16316 of 16384 graded samples
  changed (`work/reports/calibrate_1e5.log`). Earlier probes at 3.1e-5 and
  1.0e-3 degrees gave 1.012523e-12 and 3.266498e-11, so the response is linear
  in the perturbation and this measurement falls exactly where those two
  predict. `atol` sits ≈9x above the committed spread, which is why
  self-validation is expected to pass.
- **Physical signal that must stay visible.** The two receivers are 0.00194°
  (≈200 m) apart in the PKP precursor window and differ by ≈2.0e-3 relative to
  peak in the SAC domain, consistent with the 0.94% maximum complex-modulus
  difference measured in the `binary64` frequency-domain stream. `atol` is well
  below that, so the check still distinguishes the two stations.

The self-validation fields in `rubric.json` carry the bare-metal measurement of
the committed variant until the container self-check writes back its own; none
of the figures above is an extrapolation.

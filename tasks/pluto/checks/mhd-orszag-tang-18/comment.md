# Numerical-tolerance narrative

This check reproduces the official PLUTO `MHD/Orszag_Tang` configuration 18 without changing its physics, grid, HLLD solver, linear reconstruction, RK2 update, constrained-transport update, input deck, or output cadence.

## Final validation policy

- **Field arrays:** maximum absolute difference `<= 2e-8` over every named raw-FP64 value in every post-initial frame.
- **Time and timestep:** relative difference `<= 1e-6` for the decimal values written to `dbl.out`.
- **Frame indices and step numbers:** exact equality.
- **Conformance and finiteness:** required before numerical comparison.

The initial `data.0000.dbl` frame is required and checked for shape and finiteness, but it is excluded from field scoring because it is produced by initialization rather than by a numerical update.

## Why the old `1e-4` field bound was rejected

The replacement initially retained `1e-4` from the retired HD Jet check. That number had been justified using Jet-specific PVTE inversion and H2-cooling integration paths. Those paths do not exist in this ideal-MHD Orszag–Tang deck, so `1e-4` was not valid calibration for this check.

Two independent no-cache builds of the unchanged arm64 GCC, `-ffp-contract=off` image reproduced byte-identically. This proved fixed-environment determinism, but zero spread between equivalent builds does not by itself establish a portable nonzero tolerance.

## Why `1e-10` was tested and rejected

PLUTO stores the field state, simulation time, and timestep as C `double`, and the graded field files are raw little-endian float64 arrays. That made `1e-10` a plausible candidate, but FP64 storage alone does not measure accumulated algorithmic roundoff.

A controlled correct-solver experiment therefore changed only GCC floating-point contraction:

- incumbent: `-ffp-contract=off`;
- candidate: `-ffp-contract=fast`;
- all source, physics, grid, HLLD, RK2, constrained transport, input, and output settings unchanged;
- the candidate binary contained 994 fused-multiply-add instructions.

Both runs completed with the same time, timestep, and exact step sequence `[0, 750, 1685]`. The initial frame was bitwise equal, but the evolved frames differed. Across all three frames and all 1,967,616 finite float64 values, the measured maximum absolute field difference was:

```text
1.1553805134845163e-9
```

The worst value was pressure in frame 2 at zero-based location `(i=125, j=191, k=0)`. The all-value RMS difference was `1.7411725010509582e-12`. Therefore `1e-10` fails this valid correct-solver contrast by a factor of `11.5538`.

As a separate compiler cross-check, an isolated arm64 Clang 19.1.7 build changed only the compiler package and `CC`, while preserving `-O3 -std=c17 -Wundef -ffp-contract=off -D_DEFAULT_SOURCE`. Its 1,967,616 field values, `dbl.out`, times, timesteps, and steps were exactly equal to the GCC incumbent.

## Why the selected field bound is `2e-8`

The measured correct-run maximum is `1.1553805134845163e-9`. The calibration policy requires at least a tenfold margin above the largest measured correct-run spread:

```text
10 * 1.1553805134845163e-9 = 1.1553805134845163e-8
```

The clean selected bound, `2e-8`, is `17.3103` times the measured maximum. It is far below the separately measured wrong-solver difference of `0.7706189583084884` from replacing HLLD with HLL; that defect experiment is evidence of rejection power, not tolerance calibration.

## Why time and timestep use `1e-6`

PLUTO computes time and timestep internally as FP64, but `dbl.out` writes them as decimal text with seven significant digits. The validator can only compare the emitted representation, so a relative `1e-6` rule matches the precision available in that file. Step numbers remain exact and independently fingerprint the timestep trajectory.

## Scope and remaining limitation

The `2e-8` field bound is calibrated for the measured arm64 CPU compiler/FMA contrasts. It is **not yet GPU-calibrated**. A native amd64 experiment was also unavailable on the measurement host because Docker had neither binfmt nor buildx support; no Docker configuration was changed to manufacture that evidence.

Before describing this tolerance as a general CPU/GPU portability guarantee, run the unchanged FP64 solver on the intended GPU implementation and additional native architectures, compare all raw fields and exact step trajectories, and expand the bound only if those correct-run measurements require it.

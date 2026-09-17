# Free-stream — uniform-equilibrium field-solver test

**Test directory:** `tests/freestream/`

A 1D periodic **free-stream**: the initial condition is a *perfectly uniform*
plasma (uniform number density, zero bulk velocity, uniform guide field `Bx0`
along x) on a coarse grid. The only structure is PIC noise. A correct field solver must keep this trivial state **exactly steady**: any growth in the magnetic or particle energy is purely numerical error or shot-noise.

## Configuration

The directory holds two parameter files that differ only in the solver block:

| File | Field solver | Ohm's-law terms |
| --- | --- | --- |
| `PARAM.in` | Full PIC implicit Maxwell/GMRES (`#SOLVEEM T`, `#HYBRIDPIC F`) | n/a (full E–B solve) |
| `PARAM.in.hybrid` | Hybrid (Ohm + Faraday) (`#SOLVEEM F`, `#HYBRIDPIC T`, `#HALLTERM F`) | convection only (no sub-cycling) |

## Validation

Both variants are checked by the shared `validate_hybrid`: the magnetic energy
`Eb` and ion energy `Epart` must stay steady from the first to the last logged
step.

## Physics scales

- Normalization chosen so `d_i ~ v_A ~ 1`: `lNormSI = 1e5 m` (= `d_i`),
  `uNormSI = 1e5 m/s` (= `v_A`), so `tNorm = lNormSI/uNormSI = 1 s` and
  `#STOP TimeMax` (in s) equals the code time in units of `Ω_i⁻¹`.
- Single kinetic ion species (`q = m = 1`), `n = 5 /cc`, uniform guide field
  `Bx0 = 1.044e-8 T` (~10.4 nT; `Ω_i = 1.0 rad/s`, gyroperiod `T_ci = 2π ≈ 6.3 s`),
  `T = 10000 K`.
- Grid spans `Lx = 6.4 d_i` with 32 cells (periodic, 1 cell in y/z).
- Fixed `dt = 0.02` (`dt·Ω_i = 0.02`) for `tmax = 10 s` = 10 code units ≈ 1.6
  gyroperiods.

## Relationship to other tests

- `whistler/`, `ohm/` — seeded wave tests built on the same equilibrium.
- `whistler_solveem/` — full-PIC *wave* test.

## Running

Both variants run together under the test name:

```bash
python3 tests/validate_tests.py --test=freestream          # both variants
```

To run a **single variant**, append its token to the test name:

```bash
python3 tests/validate_tests.py --test=freestream.full      # full PIC only (PARAM.in)
python3 tests/validate_tests.py --test=freestream.hybrid    # hybrid only (PARAM.in.hybrid)
```
# HYBRID_WHISTLER

Parallel whistler–Alfvén wave test in FLEKS. This directory now runs **two
variants of the same eigenmode**:

| Variant | Config file | Field solver | Species |
| --- | --- | --- | --- |
| **Full PIC** | `PARAM.in` | Maxwell/GMRES (`solveEM=T`) | kinetic ions **and** kinetic electrons |
| **Hybrid** | `PARAM.in.hybrid` | generalized Ohm's law (`useHybridPIC=T`) | kinetic ions + fluid electrons |

Both variants are seeded with the same x-aligned, transverse, circularly
polarized whistler wave (`#TESTCASE HybridWave`, `frac=0.02`) and the same
Alfvén velocity kick, so they exercise the identical whistler eigenmode with
two different field/B solvers. This lets the full-PIC dispersive result (with
kinetic electrons) be compared directly against the Hall-only hybrid result.

## Physics / normalization

All quantities are in normalized CGS/code units chosen so ion scales are O(1):

* Box length `Lx = 6.4 d_i` (`nCellX = 64`, `maxBlockSizeX = 64`).
* `lNormSI = d_i ≈ 1.0e5 m`, `uNormSI = 1.0e5 m/s`, so `tNorm = lNormSI/uNormSI
  = 1 s`.  Hence `#STOP TimeMax` (always SI seconds) equals the code time in
  units of `Ω_i⁻¹` directly: `TimeMax = 13.0 s = 13 Ω_i⁻¹ ≈ one whistler period`.
* With `q/(m c) = 1` for the proton, `B = Ω_i ~ 1` in code units.
* Timestep `dt = 0.02` → `dt·Ω_i = 0.02 ≪ 1`, numerically stable for the
  whistler mode (bounded by `Ω_i`, since
  `ω/Ω_i = (k d_i)² / (1 + (k d_i)²)`).

### Whistler eigenmode in SI units

For the `n = 1` mode (`k d_i = 2π/6.4 = 0.982`):

* Guide field `B = 1.044e-8 T ≈ 10.4 nT`, density `n = 5 amu/cc`
  (`ρ = 8.36e-21 kg/m³`).
* Ion gyrofrequency `Ω_i = qB/m_i = 1.00 rad/s` (gyroperiod `T_ci = 2π ≈ 6.3 s`).
* Alfvén speed `v_A = B/√(μ₀ρ) ≈ 1.02e5 m/s ≈ 102 km/s`.
* Ion inertial length `d_i = c/ω_pi ≈ 1.02e5 m ≈ 102 km` (`≈ lNormSI`).
* Whistler frequency `ω = Ω_i (k d_i)²/(1+(k d_i)²) ≈ 0.49 rad/s`
  → **period `T = 2π/ω ≈ 12.8 s`**.
* `TimeMax = 13 s` ≈ one whistler period — enough frames to fit the phase and
  measure the frequency.

In the full-PIC variant the kinetic electron species has
`q = -1`, `m = 1/1836` (proton mass ratio), so the PIC Maxwell solve carries
the full electron+ion whistler dispersion. The guide field and electron density
match the ions (`n_e = n_i`).
This real electron mass does **not** affect the timestepping in the test:
the timestep here is fixed (`useFixedDt = T`, `dt = 0.02`) and is set by the
ion-scale whistler frequency (`ω/Ω_i ≲ 1`, i.e. `dt·Ω_i ≪ 1`).

## Running

Both variants run together under the test name: `PARAM.in` as **FULL PIC** and
`PARAM.in.hybrid` as **HYBRID**.

```
python3 tests/validate_tests.py --test=whistler          # both variants
```

To run a **single variant**, append its token to the test name:

```
python3 tests/validate_tests.py --test=whistler.full      # full PIC only (PARAM.in)
python3 tests/validate_tests.py --test=whistler.hybrid    # hybrid only (PARAM.in.hybrid)
```

## Validation

`validate.py` runs the whistler field checks for **both** variants: it measures
the whistler frequency from the transverse B perturbation and compares it with
the analytic dispersion `ω/Ω_i = (k d_i)² / (1 + (k d_i)²)`, and checks the
Alfvén-speed propagation. For the **hybrid** variant it additionally runs the
shared hybrid energy-log checks (see the [hybrid README](../_shared/hybrid.md)).

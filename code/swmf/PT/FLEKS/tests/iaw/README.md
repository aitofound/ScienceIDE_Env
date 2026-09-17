# Ion Acoustic Wave (IAW) test

A hybrid-PIC regression test for an ion acoustic wave seeded as a single
sinusoidal ion density perturbation in a uniform, Maxwellian, zero-bulk-velocity
proton plasma.  The setup is matched to the Hybrid-VPIC `iaw` reference so the
decay rate can be compared directly.

## Physics

The initial condition is

```
  n(x) = n0 * (1 + pert * sin(kx * x)),    kx = 2*pi*waveMode / Lx
  u_i  = 0   (Maxwellian about zero)
  E    = 0
  B    = 0   (unmagnetized)
```

with `pert = 0.02` and one wavelength across the domain (`waveMode = 1`).  The
restoring force is the electron pressure-gradient ("ambipolar") term in the
generalized Ohm's law

```
  E = -(1/rho) ∇P_e,        P_e = P0 (rho/rho0)^gamma   (adiabatic, gamma = 5/3)
```

With `B = 0` the Hall term `(J x B)/rho` and the convection term `-u_i x B`
both vanish, so this is the **only** electric-field source.  The wave is a
purely electrostatic, collisionless ion acoustic wave whose only damping
mechanism is **ion Landau damping** (`eta = 0`, `hypereta = 0`).  Because the
field is unmagnetized and the Hall term is switched off, this test isolates the
electron-pressure (ambipolar) branch of the Ohm's law and the ion-Landau damping
of a pure electrostatic IAW.

The parameters are matched to Hybrid-VPIC `examples/iaw`:

| Quantity | Reference | FLEKS (this test) |
|---|---|---|
| Magnetic field | `B = 0` (b0 is only a scale) | `B = 0` |
| Electron polytropic index | `gamma = 5/3` | `5/3` |
| Sound speed `c_s` | 1.0 | `~1` (`electronTemperature` = 62.8 eV at `uNormSI = 1e5`) |
| Ion thermal speed `v_thi` | `sqrt(1/3) = 0.577` | `~0.58` (T = 4e5 K) |
| `c_s / v_thi` | 1.73 | 1.73 (same Landau damping) |
| `Lx [d_i]` | 16 | 16 |
| `k d_i` | `2*pi/16 = 0.393` | `2*pi/16 = 0.393` |
| Density perturbation | 0.02 | 0.02 |
| Run time [code units] | 50 | 5 (fast regression) |
| `dt` | 0.02 | 0.02 |

The time axis is in code units: with `lNormSI = uNormSI = 1.0e5`, `tNorm =
lNormSI/uNormSI = 1 s`, so `#STOP TimeMax` (in s) equals the code time directly.
The test is unmagnetized (`B = 0`), so `Ω_ci = 0` and "`omega_ci^-1`" would be
meaningless; the physical reference is the n=1 ion-acoustic period
`T_iaw = 2π/(k c_s) = 2π/0.393 ≈ 16 s`, so `TimeMax = 5 s ≈ 0.3 T_iaw`.
(SI: `c_s = 1.0e5 m/s`, `Ω_ci` undefined; ion acoustic frequency
`ω = k c_s ≈ 0.393 rad/s`.)

> **Fast regression:** the standalone test runs to `TimeMax = 5` with
> `nParticle = 2000` per cell so it finishes in ~1 minute on a single core
> (48 cells x 2000 = 96k ions, 250 time steps). This is enough to exercise the
> ambipolar field, the stability / mass conservation, and the
> `centerPlasmaPrev` ghost-cell fix (no grid-scale checkerboard). A full
> `TimeMax = 50`, `nParticle = 150000` run reproduces the Landau damping curve
>  in Hybrid-VPIC.

## Implementation

The density perturbation is seeded by the `IonAcousticWave` test case (in
`Pic` / `Particles`): each loaded particle's weight is multiplied by
`(1 + pert * sin(kx * x))`, so the deposited ion density (charge/mass/pressure
moments) carries the sinusoidal profile while the bulk velocity stays uniform.

Activate it in `PARAM.in` with

```
#TESTCASE
testCase  IonAcousticWave
pert      0.02
waveMode  1
```

`pert` is the relative density-perturbation amplitude (must be `< 1`) and
`waveMode` is the number of wavelengths across the x-domain.

## Run

```
python3 tests/validate_tests.py --test=iaw
```

## Validation

1. The seeded density profile is a clean single-mode sinusoid (`rhoS0` in the
   plot output);
2. The **ambipolar electric field `Ex` is non-zero at the last plot frame**
   (the initial `Ex` is zero by construction; a non-zero late-time `Ex` guards
   against the structured plot reading a stale/zero node-centred E)
3. The density-perturbation amplitude stays bounded (the IAW damps rather than
   blowing up).

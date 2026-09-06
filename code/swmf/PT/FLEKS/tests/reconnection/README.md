# Magnetic Reconnection Standalone Test

This test demonstrates **magnetic reconnection** in FLEKS, reproducing the
physics of the Hybrid-VPIC *"islands"* force-free Fadeev current-sheet example
(`examples/islands/`).  It runs under **two field solvers**:

- **`PARAM.in.hybrid`** — the hybrid solver (kinetic ions + massless fluid
  electrons, generalized Ohm's law).
- **`PARAM.in`** — the full-PIC solver (kinetic ions + kinetic electrons,
  `m_i/m_e = 25`, standard Maxwell/GMRES EM solve).

Both share the same `FadeevIC` initial condition (`#TESTCASE fadeev`,
`src/ic/FadeevIC.h/.cpp`), which seeds the Fadeev equilibrium fields and the
island density profile, plus the m=1 perturbation that drives the reconnection.

## Coordinate mapping

FLEKS's *fake-2D* convention keeps one cell along `z`, so the reconnection
plane is `x-y` with `x` the periodic drive direction, `y` the current-sheet
normal (the Fadeev profile coordinate), and `z` the thin invariant direction.

## Equilibrium

In code units (`d_i ~ v_A ~ 1`), with `L = 5 d_i`, `eps = 0.4`,
`num_islands = 2`, guide field `bg = 0` (anti-parallel):

```
Bx = b0*sinh(y/L)/(cosh(y/L) + eps*cos(x/L))   + m=1 perturbation
By = b0*eps*sin(x/L)/(cosh(y/L) + eps*cos(x/L))+ m=1 perturbation
Bz = b0*bg                                     (= 0)
n(x,y) = nb + (1-nb)*profile(x,y)/profile_max,
    profile(x,y) = (1-eps^2)/(cosh(y/L) + eps*cos(x/L))^2,
    profile_max = (1+eps)/(1-eps)  (at the island O-points, x = +-pi*L).
```

The fields are set by `FadeevIC::set_fields`.  The plasma is loaded uniform
(`#UNIFORMSTATE`, density `nb = 0.2`, ion `beta_i ~ 0.5`) and
`FadeevIC::modify_particle_weight` boosts each macroparticle's weight to the
Fadeev sheet profile `n(x,y) = nb + (1-nb)*profile/profile_max`.

For the full-PIC (two kinetic species) case the load must be charge neutral and
force balanced.  The `#UNIFORMSTATE` density is a mass density converted to a
number density by `n_s = rho_s / (m_s/m_i)`, so the electron mass density must
be `rho_e = rho_i*m_e/m_i = 12.5*0.04 = 0.5` to give `n_e = n_i`.
`FadeevIC::modify_particle_velocity` seeds the diamagnetic drift
`u_s = -(T_s/q_s) (grad(n) x B)/(n B^2)` so that `J x B = grad(p)` at `t = 0`.

`#PERIODICITY` is periodic in `x` and `z`; the cross-sheet `y` faces use
`#FIELDBOXBOUNDARY outflow` (float B).  The current sheet is far from the
`y`-walls (`Ly/2 = 15.7 d_i` vs `L = 5 d_i`), so the walls do not affect the
interior reconnection.

## Domain and resolution

Box: `Lx = 2*pi*L*num_islands ~ 62.8 d_i`, `Ly = Lx/2 ~ 31.4 d_i`, thin `z`.

Normalization: `lNormSI = uNormSI = 1.0e5`, so `tNorm = lNormSI/uNormSI = 1 s`;
the guide/reconnection field is set directly in code units by `FadeevIC b0 = 1`
(so `Ω_i = 1` code, `TimeMax` in s == code time in `Ω_i⁻¹`).  SI scales
(physical input `n = 12.5 amu/cc` background, `beta_i ~ 0.5`): `Ω_i = qB/m_i`
with `B = 1.0` code, `v_A ~ 1.0e5 m/s`, `d_i ~ 1.0e5 m`, ion gyroperiod
`T_ci = 2π ≈ 6.3 s`, Alfvén transit across `Lx` ~ `6.3e5 m / 1e5 m/s ≈ 6.3 s`.

- **Hybrid** (`PARAM.in.hybrid`): `64 x 32 x 1`, `nppc = 36` ions, `dt = 0.02`,
  `TimeMax = 20 s = 20 Ω_i⁻¹ ≈ 3.2 gyroperiods` (1000 steps).
- **Full-PIC** (`PARAM.in`): `32 x 16 x 1`, `nppc = 25` per species,
  `dt = 0.005` (resolves the electron timescale for `m_i/m_e = 25`),
  `TimeMax = 3 s = 3 Ω_i⁻¹ ≈ 0.5 gyroperiod` (600 steps).

Both are sized to finish within the 1-minute standalone serial budget and also
run on a few MPI processes.

## Running

Run the automated check (builds, runs both solver variants, post-processes, and
validates):

```bash
# serial (default) -- runs both full PIC and hybrid
python3 tests/validate_tests.py --test=reconnection

# or with MPI (e.g. 2 processes)
python3 tests/validate_tests.py --test=reconnection -n 2
```

To run a **single variant**, append its token to the test name:

```bash
python3 tests/validate_tests.py --test=reconnection.full     # full PIC only (PARAM.in)
python3 tests/validate_tests.py --test=reconnection.hybrid   # hybrid only (PARAM.in.hybrid)
```

## Validation

The automated check (`validate.py`) runs for both solver variants and reads the
`z=0` x-y plane `.out` files to verify:

1. **Energy-log sanity**: finite `Eb` / `Epart`, bounded `Eb` (no blow-up).
2. **Equilibrium init** (t=0): the in-plane field nulls (O-points, the island
   centres) sit at `x ~ +-pi*L ~ +-15.7 d_i`, the sheet density peaks near 1
   and the background is near 0.2.
3. **Active reconnection**: the seeded m=1 perturbation grows (`max|delta By|`
   grows by several-fold) and the out-of-plane flux function `Ay` at the
   central X-point gives a non-trivial reconnection rate `dAy/dt`.
4. **O-point evolution**: the island-centre positions evolve, confirming the
   magnetic topology changes (reconnection) rather than remaining frozen.
5. **Charge neutrality (full-PIC only)**: the load must stay quasi-neutral,
   i.e. `rhoS0 ~ rhoS1*(m_i/m_e)`, catching the "electron `rho` not scaled by
   `m_e/m_i`" bug.  Skipped for the hybrid run (no electron species).

These are qualitative physics checks; they confirm both solvers reproduce the hallmarks of reconnection in the Fadeev equilibrium.

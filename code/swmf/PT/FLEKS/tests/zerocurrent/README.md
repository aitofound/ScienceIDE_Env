# Zero-Current Hybrid Wave Test (`tests/zerocurrent/`)

## Purpose

Isolates the **current dependence** of the Hall term in the hybrid solver.  The
domain is seeded with a *sinusoidal transverse B perturbation* riding on a uniform
guide field `Bx0` (via the generic `WaveIC` plug-in, `#TESTCASE waveic`), but
**no macroparticles are loaded** (`nParticle per cell = 0 0 0`).

With no particles the ion charge/mass density `rho` is identically zero
everywhere.  In the hybrid field solver the generalized Ohm's law is fully
short-circuited by the `rho > 0` guard, so every cell is left inert and
contributes no electric field, i.e. `E = 0`.  Faraday's law then gives

```
dB/dt = - curl E = 0,
```

so the B perturbation is **frozen** and does **not** propagate — even though the
curl of the sinusoidal perturbation is non-zero (a real whistler/MHD mode would
rotate it).  Mechanically the Hall term `(J x B)/rho` is *never evaluated* because
`rho = 0`; physically this is the zero-current regime — with no plasma there is no
current source to drive the field.  (Note the distinction from the single-cell
test: there the Hall term *is* computed but yields zero because `curl B = 0`; here
it is skipped entirely because `rho = 0`.)

This complements the single-cell test: there the Hall term vanishes because
`curl B = 0` (and is genuinely evaluated); here the whole Ohm's law is inert
because `rho = 0`.

## Setup

* 64x1x1 periodic Cartesian grid (`Lx = 6.4 d_i`, `#NCELL 64 1 1`).
* Hybrid PIC (`#SOLVEEM F`, `#HYBRIDPIC T`), Hall term **ON** (`#HALLTERM T`).
* `#TESTCASE waveic` plus a `#WAVEIC` block that sets `seedB = 1`,
  `guideField = 1`, `waveMode = 1`, `frac = 0.5`:
  transverse `By,Bz = 0.5 * cos(kx x)` (kx = 2*pi/Lx, box fundamental) on a
  uniform guide field `Bx0 ~ 1`.
* **Zero particles** (`#PARTICLES 0 0 0`) => `rho = 0` everywhere => `E = 0`.
* Fixed `dt = 0.02`, runs to `TimeMax = 6.25`.

## Validation

`validate_zerocurrent` checks from the pic-log history that `Eb` is conserved to
round-off and that `Ee` stays ~ 0 (the perturbation is frozen, neither
propagating nor decaying). See `tests/validate_tests.py` for the exact
tolerances.

## Run

```
python3 tests/validate_tests.py --test=zerocurrent
```

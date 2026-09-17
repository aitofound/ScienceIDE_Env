# tests/shock

1D magnetized collisionless shock test for FLEKS: a super-magnetosonic plasma
streams into a reflecting wall, exercising a reflecting wall and an upstream
inflow boundary in the same deck.  Two solver variants are provided:

* **`PARAM.in`** — the **FULL PIC** solver (kinetic ions + kinetic electrons,
  reduced mass ratio `m_i/m_e = 25`).
* **`PARAM.in.hybrid`** — the **HYBRID** solver (kinetic ions, massless
  electron fluid at `Te ≈ 140 eV`).

Both variants share the same physics; only the field solver (`solveEM` /
`useHybridPIC`) differs.

## Setup

The upstream is a moderately strong super-critical shock:

| quantity | value |
| --- | --- |
| shock-normal angle θ_Bn | **45°** (oblique) |
| Alfvén Mach number `M_A` | **≈ 7** (flow speed ≈ 7 `v_A`) |
| fast-magnetosonic Mach number `M_f` | **≈ 5** |
| ion plasma beta `β_i` | **≈ 1** (thermal ≈ magnetic pressure) |
| ion inertial length `d_i` | ≈ 1 code unit |
| domain / cell size | ≈ 63 `d_i` long, `dx ≈ 1 d_i` (ion-scale resolution) |
| run length | 5 s ≈ **0.8 ion gyroperiods** (`Ω_ci ≈ 1` in code units) |

At `M_f ≈ 5` the shock is well beyond the second critical Mach number, so ion
reflection drives a genuine super-critical shock: the downstream is compressed
by a factor of a few and the wall region carries the reflected-ion foot.

* Boundaries:
  * **-x (left) wall = `outflow` (field) + `reflect` (particles)**: the ghost
    EM field is a zero-gradient copy of the edge cell, so the wall imposes no
    constraint -- the tangential motional E `E = -u x B` stays continuous and B
    is free to compress as the shock forms.  This is the physical pairing with a
    reflecting wall: `conducting`/`symmetry` would force `E_t = 0` or `B_n = 0`
    (wrong for an oblique flowing plasma), and `fixed` would pin a compressed
    wall to the upstream state.
  * **+x (right) = `fixed` field + `inflow` particles**: the upstream plasma
    state is supplied by `#INFLOW` through the flux-weighted particle injection
    each step, while the ghost EM field is Dirichlet-pinned to the uniform state.
  * y/z = `periodic` (single-cell).

## Validation

Both variants are validated identically by `validate.py` (see its module
docstring for the regression context), and the harness requires a clean exit
code 0 through `TimeMax`.  Two checks:

### PIC energy log

* `Etot`, `Ee`, `Eb`, `Epart` finite every frame (no NaN/Inf).
* `Eb` and `Epart` growth ratios within `[0.2, 1e3]` (bounded; no divergence).
* `Ee` below `1e2 * Eb` (no spurious electric-field build-up).

This guards the regression the boundary fix removed: the original
`conducting`/PEC left wall forced `E_t = 0`, incompatible with a flowing
magnetized plasma (the motional E `E = -u x B` needs non-zero tangential E).
With that wall the **field** energy `Eb` diverged first (18.5 -> 6.1e4 within
~960 steps) while `Epart` stayed matched to its initial value until the end --
a field-driven, not particle-driven, blow-up.  The wall now uses `outflow`
(zero-gradient), which leaves the tangential motional E `-u x B` intact.

### Final snapshot

With `ux < 0` the inflow is at `+x`, so the rightmost 25 % is the **upstream**
region and the leftmost 50 % is the **downstream** (wall) region.  Checks are
self-consistent (the upstream target is read from the snapshot's own steady
inflow region):

* **Inflow upstream stable:** `Bx`, `rhoS0`, `uxS0`, `Ey`, `|B|` steady and
  uniform upstream (relative std < 0.15) -- the boundary field is not eroded and
  the injected state does not drift.
* **Shock formed:** downstream mean `rhoS0` (> 1.2×) and `|B|` (> 1.1×) exceed
  the upstream means, and peak density reaches at least `1.5×` the upstream mean
  (a clear jump, not uniform flow).  Any NaN/Inf in the wall region also fails.

## Running

```
python3 tests/validate_tests.py --test=shock          # both (PARAM.in + hybrid)
python3 tests/validate_tests.py --test=shock.full     # full PIC only (PARAM.in)
python3 tests/validate_tests.py --test=shock.hybrid   # hybrid only (PARAM.in.hybrid)
```

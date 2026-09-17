# Recombination Loss Test (O2+ + e- -> O + O)

## Description

This standalone test verifies the `#RECOMBINATION` command for dissociative
recombination of O2+ ions with electrons. The recombination process removes
O2+ ions (and, by quasi-neutrality, electrons) from the plasma, converting
them to neutral O atoms. This test exercises the **particle weight reduction**
mechanism in `Particles::apply_loss()`, which is the loss-term counterpart
to the source-term particle injection.

## Physics & Solver Setup

- **Geometry & Boundaries**: 1D periodic grid (32 cells in X, 1 in Y, Z),
  domain spans [-9.0x10^6, 9.0x10^6] m (Mars-like scale).
- **Plasma Species**:
  | Species | Mass [amu] | Charge [e] | Role                          |
  |---------|------------|------------|-------------------------------|
  | 0 (e-)  | 0.01       | -1         | Electron (quasi-neutral)      |
  | 1 (H+)  | 1.0        | +1         | Background ion (no recombination) |
  | 2 (O2+) | 32.0       | +1         | Recombining ion (weight decreases) |

  Quasi-neutral electron density: ne = n_H + n_O2 = 5 + 0.156 = 5.156 /cc,
  giving rho_e = 0.0515625 amu/cc.

- **Recombination**: Enabled via `#RECOMBINATION` command. One reaction:
  - O2+ + e- -> O + O
  - Rate coefficient: k0 = 7.38x10^-3 cm^3/s (inflated 1e5x from the
    BATSRUS ModUserMars value of 7.38x10^-8 cm^3/s to produce a detectable
    signal in the short 20-step simulation)
  - Temperature exponent: 0 (no Te dependence; avoids the #UNIFORMSTATE
    pressure normalization issue that makes Te unreliable in standalone tests)
  - The BATSRUS formula is k = k0 * (Tref/Te)^alpha with Tref=1200 K, alpha=0.56.

  The loss rate for O2+ is: `L_rho = k * ne * rho_O2+` [kg/m^3/s].
  This is stored in `nodeLossFluid` and applied by `apply_loss()` which
  reduces each O2+ particle's weight by the fraction:
  `fraction = min(L_rho * dt / rho_existing, 1.0)`.

- **No ionization sources**: Photoionization, electron impact, and charge
  exchange are all disabled. The only process active is recombination loss,
  so the O2+ density should monotonically decrease.

- **Electromagnetic Fields**: Enabled (`solveEM = T`) with guiding field
  Bx = 5.0x10^-9 T.

## Validation

The validator checks that O2+ kinetic energy decreases (recombination removes
O2+ by reducing particle weights) while H+ energy stays stable (recombination
only affects O2+). See `tests/recombination/validate.py` for the exact
tolerances.

## Running

From the FLEKS root directory (requires compiled `bin/FLEKS.exe`):

```bash
python3 tests/validate_tests.py --test=recombination
```

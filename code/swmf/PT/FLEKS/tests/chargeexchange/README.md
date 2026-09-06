# Charge Exchange Source Test (H + O, Chamberlain Neutral Profile)

## Description

This standalone test verifies the `#CHARGEEXCHANGE` command for charge exchange
ionization of analytical neutral exosphere density profiles with **two neutral
components** (H and O) using the **Chamberlain model**. Each neutral component
exchanges charge with **all ion species** (H+ and O+); the resulting source
injects ions into the PIC simulation domain.

## Physics & Solver Setup

- **Geometry & Boundaries**: 1D periodic grid (32 cells in X, 1 in Y, Z),
  domain spans [-9.0×10^6, 9.0×10^6] m (Mars-like scale).
- **Exosphere Model**: Chamberlain profile with 2 neutral components:
  | Component | n0 [m^-3] | H0 [m]   | T0 [K] | Description                  |
  |-----------|-----------|----------|--------|------------------------------|
  | H         | 1.0×10^10 | 2.0×10^6 | 300    | Light species, large scale height |
  | O         | 5.0×10^10 | 3.0×10^5 | 300    | Heavy species, small scale height |

  Density: \( n(r) = n_0 \exp\left[-H_0\left(\frac{1}{R_p} - \frac{1}{r}\right)\right] \) for \( r \geq R_p \).
  Planet radius \( R_p = 3.0 \times 10^6 \) m (Mars-like).

- **Ionization**: Charge exchange, enabled via `#CHARGEEXCHANGE` command.
  The number of neutral components is inherited from `#EXOSPHERE`
  (so `#EXOSPHERE` must precede `#CHARGEEXCHANGE`); the command declares
  only `nIonSpecies` and the `nComponent × nIonSpecies` cross-section
  matrix.  Uses a constant cross-section model:
  \( \langle\sigma v\rangle = \sigma_{\text{CX}} \cdot u_{\text{rel}} \),
  where \( u_{\text{rel}} \) is the ion-neutral relative speed.  The
  neutrals are assumed to be at rest (zero bulk velocity), so the ion
  speed alone is used as the relative speed.  Each neutral component can
  exchange charge with **every** ion species; the total charge-exchange
  frequency for neutral component \(i_C\) is summed over all ion species:
  \( \nu_{\text{CX}}(i_C) = \sum_{iSp} n_{iSp} \cdot \sigma_{\text{CX}}(i_C, iSp) \cdot u_{iSp} \).
  The cross-section matrix (rows = neutral component, columns = ion species):
  | Neutral \ Ion | H+ (sp 1)    | O+ (sp 2)    |
  |---------------|--------------|--------------|
  | H (comp 1)    | 2.0×10^-12   | 1.0×10^-12   |
  | O (comp 2)    | 1.5×10^-12   | 1.0×10^-12   |

  The cross-sections are artificially inflated (realistic values are
  ~10^-15 cm^2) to produce a detectable source signal within the short
  10-step simulation.
  Source mass rate per mapped ion species \(i_C+1\):
  \( S_{\rho,i_C+1} = n_{i_C}(r) \cdot \nu_{\text{CX}}(i_C) \).

- **Plasma Species**:
  | Species | Mass [amu] | Charge [e] | Role                          |
  |---------|------------|------------|-------------------------------|
  | 0 (e-)  | 0.01       | -1         | Electron (quasi-neutral)      |
  | 1 (H+)  | 1.0        | +1         | Background ion (ux=-400 km/s) |
  | 2 (O+)  | 16.0       | +1         | Near-zero background (tiny seed, receives source) |

  The O+ background density is set to 1.0×10^-12 amu/cc (essentially zero)
  so that any O+ particles observed in the simulation are produced by the
  charge exchange source, making the source signal clearly detectable.

- **Electromagnetic Fields**: Enabled (`solveEM = T`) with guiding field
  \( B_x = 5.0 \times 10^{-9} \) T.

## Validation

The validator checks from the pic-log history that O+ energy grows strongly (the
O+ background is near-zero, so all O+ comes from the source) while the H+
population stays stable, and from the plot output that the O+ density peaks near
the planet surface.

## Running

From the FLEKS root directory (requires compiled `bin/FLEKS.exe`):

```bash
python3 tests/validate_tests.py --test=chargeexchange
```

> **Requires the Exo user source** (`./Config.pl -u=Exo`); see
> [tests/README.md](README.md#building-the-test-executable).

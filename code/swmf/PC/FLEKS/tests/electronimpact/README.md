# Electron Impact Ionization Test (H + O, Chamberlain Neutral Profile)

## Description

This standalone test verifies the `#ELECTRONIMPACT` command for electron impact
ionization of analytical neutral exosphere density profiles with **two species**
(H and O) using the **Chamberlain model**. The impact ionization source injects
heavy ions (species 1) into the PIC simulation domain.

## Physics & Solver Setup

- **Geometry & Boundaries**: 1D periodic grid (32 cells in X, 1 in Y, Z),
  domain spans [-1000, 1000] m.
- **Exosphere Model**: Chamberlain profile with 2 neutral components:
  | Component | n0 [m^-3] | H0 [m] | Description                  |
  |-----------|-----------|--------|------------------------------|
  | H         | 1.0×10^10 | 1000   | Light species, large scale height |
  | O         | 3.0×10^10 | 100    | Heavy species, small scale height |

  Density: \( n(r) = n_0 \exp\left[-H_0\left(\frac{1}{R_p} - \frac{1}{r}\right)\right] \) for \( r \geq R_p \).
  Planet radius \( R_p = 500 \) m.

- **Ionization**: Electron impact ionization, enabled via `#ELECTRONIMPACT`
  command. Uses the Voronov 1997 rate coefficient:
  \( \langle\sigma v\rangle = A \cdot t^K / (X + t) \cdot \exp(-E_i/T_e) \),
  where \( t = T_e / E_i \).
  | Component | eIon [eV] | A [cm^3/s] | K     | X    |
  |-----------|-----------|------------|-------|------|
  | H         | 13.6      | 3.0×10^-8  | 0.39  | 0.23 |
  | O         | 13.6      | 1.0×10^-8  | 0.5   | 0.3  |
  Source mass rate per component:
  \( S_{\rho,i} = n_i(r) \cdot n_e \cdot \langle\sigma v\rangle_i(T_e) \).

- **Plasma Species**:
  | Species | Mass [amu] | Charge [e] | Role                       |
  |---------|------------|------------|----------------------------|
  | 1 (H+)  | 1.0        | +1         | Hot background (T=100 kK) for electron supply |
  | 2 (O+)  | 16.0       | +1         | Heavy ions (gets exosphere source) |

- **Electromagnetic Fields**: Enabled (`solveEM = T`) with guiding field
  \( B_x = 5.0 \times 10^{-9} \) T.

## Validation

The validator checks that species 2 (O+) particle count and kinetic energy grow
monotonically over time, confirming the electron-impact ionization source is
active.

## Running

From the FLEKS root directory (requires compiled `bin/FLEKS.exe`):

```bash
python3 tests/validate_tests.py --test=electronimpact
```

> **Requires the Exo user source** (`./Config.pl -u=Exo`); see
> [tests/README.md](README.md#building-the-test-executable).

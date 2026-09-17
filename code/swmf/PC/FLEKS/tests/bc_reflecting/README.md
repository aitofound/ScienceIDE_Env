# Reflecting & Conducting Wall Boundary-Condition Tests

This directory groups tests for reflecting particle boundaries (`#PARTICLEBOXBOUNDARY reflect`)
and perfectly conducting field walls (PEC, `#FIELDBOXBOUNDARY conducting`).

## Variants

| Variant | File | Physics | Validation |
|---------|------|---------|------------|
| `bc_reflecting_fields` | `PARAM.in.fields` | Tophat EM pulse (Ey/Bz) reflecting off conducting (PEC) x-faces (`#SOLVEEM T`) | EM energy conserved ($0.8 \le E_1/E_0 \le 1.25$); fields bounded |
| `bc_reflecting_hybrid_fields` | `PARAM.in.hybrid.fields` | Transverse shear-Alfvén wave (`HybridWave`) in a PEC cavity (`#HYBRIDPIC T`) | Run finite and stable; fields bounded near walls |
| `bc_reflecting_particles` | `PARAM.in.particles` | Kinetic ions streaming toward +x wall specularly reflect (pure particle push, `solveEM = F`) | Particle kinetic energy conserved ($E_{part} > 50\%$); particles confined |
| `bc_reflecting_hybrid_particles` | `PARAM.in.hybrid.particles` | Kinetic ions streaming toward +x wall specularly reflect in hybrid mover (`useHybridPIC = T`) | Particle kinetic energy conserved ($E_{part} > 50\%$); particles confined |

## Physics Setup

### Particle Reflection (`PARAM.in.particles` / `PARAM.in.hybrid.particles`)
- **Domain**: a closed box in x (`8×1×1` cells, `Lx = 6.4 d_i`), fake-2D periodic in y/z. Both x-faces are **reflecting**.
- **Plasma**: a single kinetic ion species (`q = m = 1`) streaming toward the `+x` wall (`ux > 0`) with an oblique `uy != 0`.
- **Specular Reflection**: $v_n \to -v_n$, position mirrored $x \to 2\cdot x_w - x$.
- **Validation**:
  1. **Energy log**: $E_{part}$ does not drain toward zero (stays $> 50\%$ of initial).
  2. **Plot output**: ion density $\rho_{S0}$ stays positive and confined across domain and boundary cells; normal velocity $ux_{S0}$ is finite.

### Conducting Field Walls (`PARAM.in.fields` / `PARAM.in.hybrid.fields`)
- **`PARAM.in.fields` (Full-PIC)**:
  - An interior tophat EM pulse (Ey/Bz step) propagates outward and reflects back and forth between PEC boundaries ($E_t = 0, B_n = 0$).
  - **Validation**: unlike absorbing boundaries (where energy decays $< 25\%$), conducting walls reflect the pulses and strictly conserve total electromagnetic energy ($E_{tot}$ conserved within discretization bounds, $0.8 \le E_1/E_0 \le 1.25$).
- **`PARAM.in.hybrid.fields` (Hybrid-PIC)**:
  - A transverse shear-Alfvén wave (`HybridWave`) in a finite-x domain with conducting field walls and reflecting particle walls.
  - **Validation**: verifies that the hybrid cell-centred field advance (`assemble_ohm_E` and `apply_centerB_BC`) stays finite, positive, and bounded without numerical instability.

## Running

All four variants run together under the test name:

```bash
python3 tests/validate_tests.py --test=bc_reflecting                      # all 4 variants
```

To run a **single variant**, append its token to the test name:

```bash
python3 tests/validate_tests.py --test=bc_reflecting.fields               # full-PIC conducting field wall
python3 tests/validate_tests.py --test=bc_reflecting.hybrid.fields        # hybrid-PIC conducting field wall
python3 tests/validate_tests.py --test=bc_reflecting.particles            # full-PIC particle reflection
python3 tests/validate_tests.py --test=bc_reflecting.hybrid.particles     # hybrid-PIC particle reflection
```

Legacy tokens `--test=bc_reflecting.full` and `--test=bc_reflecting.hybrid` are mapped to `particles` and `hybrid.particles` respectively for backward compatibility.

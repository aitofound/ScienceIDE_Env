# Wave-Injection Boundary-Condition Tests

This directory groups several wave-injection tests (`#WAVEBC`).  Each variant
is a `PARAM.in.<suffix>` file; the runner discovers all of them and runs each
once.  A single `validate.py` branches on the variant name.

## Variants

| Variant | File | Physics | Validation |
|---------|------|---------|------------|
| `bc_inject_mono` | `PARAM.in.mono` | Monochromatic Bz wave injected at the `x-lo` face into a pure-EM domain (`#SOLVEEM T`, `lightwave`) | Bz perturbation reaches the interior |
| `bc_inject_alfven` | `PARAM.in.alfven` | Shear Alfvén wave (By + matching velocity kick) injected at the `-x` face into an initially uniform magnetized hybrid plasma, propagating along +x | By reaches the interior; By/Bx < 10% (linear) |

## Validation

Both variants exercise the **wave-injection boundary condition** (`BC::wave` +
`#WAVEBC`): a wave seeded at a boundary face must propagate into the interior.
`validate.py` checks (branching on the variant name):
1. **Energy log** (both): `Etot` finite, positive, approximately conserved.
2. **`bc_inject_mono` plot**: a monochromatic transverse `Bz` wave injected at a
   boundary face propagates into a pure-EM domain; `Bz` non-zero in the interior.
3. **`bc_inject_alfven` plot**: a self-consistent shear Alfvén wave (`δBy` +
   `δv_y = -δB_y/v_A`, with `v_A ≈ 1` in code units) injected at the `-x` face
   into an initially uniform magnetized hybrid plasma propagates along `+x`;
   `By` non-zero in the interior and `By/Bx < 10%` (linear wave).

## Running

All variants run together under the test name:

```bash
python3 tests/validate_tests.py --test=bc_wave          # all variants
```

To run a **single variant**, append its token to the test name:

```bash
python3 tests/validate_tests.py --test=bc_wave.mono     # monochromatic EM wave only
python3 tests/validate_tests.py --test=bc_wave.alfven   # shear Alfvén wave only
```

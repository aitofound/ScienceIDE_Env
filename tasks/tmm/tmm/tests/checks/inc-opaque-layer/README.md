# inc-opaque-layer

Upstream test: `code/tmm/tests.py::inc_overflow_test`. Policy: `pointwise`.

## The test

`probe.py` reproduces `tests.inc_overflow_test()`: `inc_tmm` with every layer incoherent, at normal incidence and 200 nm, on a stack whose third layer of index 1+3i and 100 um thickness has a single-pass transmission of e^(-18850), floored to 1e-30 by the guard at code/tmm/tmm_core.py:772, and on the same stack truncated at that layer. R, T, the intensities `VW_list` at the start of every incoherent layer after the first and the power entering each layer are graded for both stacks; the entries behind the opaque layer are 1e-30 by construction of the floor.

Runtime knobs (`run.sh --help`): `SAB_REPEATS=1` (how many times the whole evaluation is repeated (identical graded values every time)). `SAB_CPUS=1` fixes the thread count the
NumPy libraries may use; the graded default is the declared per-check core count and is never
read from the host. The graded run takes about 0.2 s on one core (nothing is compiled).

Output files, all float64 NumPy `.npy` arrays written into the output directory; complex values are
stored as `[real, imaginary]` pairs on the last axis:

- `stack5.npy` shape `(2,)`: the five-layer all-incoherent stack: R, T of `inc_tmm`
- `stack5_vw.npy` shape `(4, 2)`: the five-layer stack: forward and backward intensities [V, W] at the start of incoherent layers 1 to 4 (`VW_list[1:]`; the package leaves `VW_list[0]` undefined as NaN, so it is not graded)
- `stack5_power_entering.npy` shape `(5,)`: the five-layer stack: `power_entering_list`, the normalised Poynting vector crossing into each layer (1 by convention for layer 0)
- `stack3.npy` shape `(2,)`: the truncated three-layer stack: R, T
- `stack3_vw.npy` shape `(2, 2)`: the truncated stack: `VW_list[1:]`
- `stack3_power_entering.npy` shape `(3,)`: the truncated stack: `power_entering_list`

## The two initial conditions

`ic/nominal/input.json` carries the upstream values verbatim: the layer indices, thicknesses, angle, wavelength and sweep ranges quoted above (complex values as [real, imaginary] pairs, the semi-infinite media as "inf").
`ic/variant/input.json` is the same file with the index of layer 1 (2.0) moved up by two binary64 ulps
(about 2.2e-16 relative), which reaches every graded value through the interface powers on both sides of layer 1 and hence R, T and every intensity; the wavelength cannot serve here because with real indices and incoherent layers nothing depends on it. Self-validation
compares the two runs to measure the check's own sensitivity; the bound must contain that spread and
still reject a real fault by a wide margin. No alternative build exists: the module is pure Python.

## The pass policy

Pointwise: |cand - ref| <= 1e-12 + 1e-09*|ref| for every graded value, applied by `validate.py` to every file above. The physical content is R, T and the power entering layers 1 and 2, which the truncated stack must reproduce and which a wrong intensity transfer matrix (code/tmm/tmm_core.py:790), a swapped T_list index or a missing floor (division by zero, NaN) moves by 1e-1 or breaks outright. The 1e-30 intensities behind the opaque layer are the floor itself, not physics; the absolute term of the bound covers them so that an implementation returning exact zeros there passes.

The bound is not the two-ulp spread: 1e-9 relative (with 1e-12 absolute for values near zero) is
six decades above the binary64 rounding floor of these closed-form Fresnel and 2x2 matrix
evaluations (the variant spread and the native reproducibility both sit at 1e-15 relative), so a
faithful double-precision port with a different summation order, fused multiply-adds or a
vectorised layout passes with room to spare, while every fault named above moves the observable by
1e-2 or more, seven decades beyond the bound.

## Evidence

Native Step 1.2 runs (Python 3.12.3, NumPy 2.5.3): inc_overflow_test gave power_entering_list[1] = 0.4210526315789473 in both stacks and finite 1.97e-30 entries behind the opaque layer, byte-identical across runs. The calibration and final
`task selfcheck` runs stamp the measured nominal-versus-variant spread and bound fraction into
`rubric.json` (`evidence`); no wrong-implementation probe beyond the fault analysis above was run.

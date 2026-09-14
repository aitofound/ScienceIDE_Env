# quspin: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

QuSpin computes exact spectra and time evolution for finite spin, boson and fermion many-body systems. This leaf owns the pinned cohesive package and exercises basis construction, operator assembly, symmetry sectors, sparse Lanczos, Floquet propagation and driven evolution. Documentation, notebooks and the separately released extension source repositories are excluded from runtime checks because they do not add independent solver paths to this pinned package.

## Build

The image installs the pinned Python package and its published extension wheels in a virtual environment. Each check runs against a copied source tree and records zero source-build seconds; the final self-validation record will supply measured run times. The extension wheels remain an explicit provenance caveat for review.

## Tolerances

Every check is graded as a two-ulp numerical calibration: nominal and variant differ only by a two-binary64-ulp change in the active field, with the physical model fixed. Measured spreads sit at 1e-14 or below against a 1e-8 bound, so the bound is a floating-point allowance rather than a physics allowance. Per-check commands, observables and tolerance rationale live in each check README and rubric.

## Coverage and exclusions

All 73 upstream `test_*.py` files are owned by a check: eight standalone baseline checks plus five grouped checks (`basis-symmetry`, `operators-projections`, `dynamics-utilities`, `entanglement-observables`, `models-crosschecks`). `comment/coverage-matrix.md` lists the file-to-check mapping.

Two upstream files (`test_Op_shift_sector.py`, `test_gen_evolve.py`) carry top-level assertions and no pytest function; the runner detects them and executes them directly so their own assertions decide pass or fail.

One upstream case is excluded and recorded: `test_quantum_operator.py::test_eigsh` compares two ARPACK `eigsh` outputs by position without sorting. The eigenvalue sets agree exactly but return in a platform-dependent order, so the assertion fails on some x86 builds. The other three cases in that file still run.

Not covered by design: notebooks, generated docs, the separately released extension source repositories, and platform-specific OpenMP build behaviour. Those are not solver paths of this pinned package.

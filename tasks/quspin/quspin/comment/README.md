# quspin: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

QuSpin computes exact spectra and time evolution for finite spin, boson and fermion many-body systems. This leaf owns the pinned cohesive package and exercises basis construction, operator assembly, symmetry sectors, sparse Lanczos, Floquet propagation and driven evolution. Documentation, notebooks and the separately released extension source repositories are excluded from runtime checks because they do not add independent solver paths to this pinned package.

## Build

The image installs the pinned Python package and its published extension wheels in a virtual environment. Each check runs against a copied source tree and records zero source-build seconds; measured run times are in `comment/pipeline/self-validation.json`. The extension wheels remain an explicit provenance caveat for review.

## Tolerances

All thirteen checks share one calibration convention: nominal uses `J=1.0,h=0.5`, and the variant moves the active binary64 bond coupling to `J=1.0000000000001` (450 ulps of 1.0, `dJ/J = 1e-13`) while the physical model stays fixed.

The perturbed input is the coupling, not the longitudinal field. Every check builds its spin-chain calibration inside a fixed-magnetization sector, where the field term `h*sum(sigma^z)` is exactly a constant times the identity: the dense matrix of `sum(sigma^z)` has diagonal spread 0 and no off-diagonal entries at every size used here. Moving `h` therefore rescales every level by the same constant instead of perturbing the spectrum, and the graded observable responds only at the eigensolver rounding level. The coupling enters the off-diagonal elements, so perturbing it is a genuine spectral change.

The step is 450 ulps rather than the conventional two. A local probe in the pinned image ran each check three ways — nominal, nominal again, and the variant — and found that a two-ulp coupling change moves the graded observable by `3.6e-15` to `5.3e-14`, at or below the repeat-to-repeat ARPACK noise floor with an unseeded start vector (measured `0` to `6.0e-14` by running the same nominal inputs twice). In 6 of the 8 standalone checks the two-ulp step is no larger than that floor, so it could not be told apart from solver noise. The 450-ulp step moves the observable by `5.6e-13` to `2.5e-11`, which is 28x to 1011x that floor.

The bound stays a floating-point allowance rather than a physics allowance. The recorded spreads are `5.6e-13` to `2.5e-11` (CI run `34929812697`), and each check's worst value uses at most `6.2e-5` of its own `atol + rtol*|x|` bound; the review table's margin column prints the reciprocal of that fraction.

Per-check commands, observables and tolerance rationale live in each check README and rubric.

## Coverage and exclusions

All 73 upstream `test_*.py` files are owned by a check: eight standalone baseline checks plus five grouped checks (`basis-symmetry`, `operators-projections`, `dynamics-utilities`, `entanglement-observables`, `models-crosschecks`). `comment/coverage-matrix.md` lists the file-to-check mapping.

Upstream's own `run_all_tests.sh` also runs two scriptable example suites, and the packaging skill counts an upstream example as an official test, so both are covered too:

- `examples/scripts/` (`example*.py`, 31 decks) by the `examples-scripts` check;
- `sphinx/doc_examples/` (`*example.py`, 33 decks) by the `basis-doc-examples` check.

Both keep upstream's pass condition — the deck must run to completion — and add one spectrum-sensitive calibration observable. The example suites are not redundant with `test/`: four production symbols (`photon.coherent_state`, `operators.commutator`, `operators.anti_commutator`, `tools.misc.get_matvec_function`) are exercised only from the example decks.

Three upstream files are excluded and recorded, none silently: `test_quantum_operator.py::test_eigsh` compares two ARPACK `eigsh` outputs by position without sorting, so a correct port can fail it depending on the platform's return order; `examples/scripts/example11.py` is a 2D exact-diagonalisation sweep that does not finish inside the check window on the declared cores; and `examples/scripts/example27.py` imports `sparse_dot_mkl`, which upstream declares only as an optional developer dependency. Two of those files' reasons are repeated in `comment/coverage-matrix.md`, and the runner records its excluded files in `observable.json` on every run.

Two upstream files (`test_Op_shift_sector.py`, `test_gen_evolve.py`) carry top-level assertions and no pytest function; the runner detects them and executes them directly so their own assertions decide pass or fail.

Not covered by design: the `examples/notebooks/` Colab suite (a tutorial surface for a hosted notebook runtime that installs its own pinned conda environment, not a solver workload of this pinned package), generated documentation, the separately released extension source repositories, and platform-specific OpenMP build behaviour.

# quspin: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

QuSpin computes exact spectra and time evolution for finite spin, boson and fermion many-body systems. This leaf owns the pinned cohesive package and exercises basis construction, operator assembly, symmetry sectors, sparse Lanczos, Floquet propagation and driven evolution. Documentation prose and the separately released extension source repositories are excluded from runtime checks because they do not add independent solver paths to this pinned package; the official example, documentation and notebook decks all run as checks.

## Build

The image installs the pinned Python package and its published extension wheels in a virtual environment. Each check runs against a copied source tree and records zero source-build seconds; measured run times are in `comment/pipeline/self-validation.json`. The extension wheels remain an explicit provenance caveat for review.

Four packages are installed for the official example decks rather than for the library: `matplotlib` and `networkx`, which upstream leaves to the user even though its example suites import them, and `mkl` with `sparse-dot-mkl`, which `example27.py` drives its solver through. The `mkl` wheel is 224 MB, and it is the only reason `example27.py` can run at all. Both Dockerfiles carry the same line, so the solver sees exactly what the oracle does.

## Runtime

Every `expected_runtime_s` is sized so that the runner classes this leaf has been
recorded on all land inside the factor-of-two band the review brief enforces. The
same suite has measured 508 s, 716 s and 925 s on three refreshes that share the
declared two cores, so the declarations sit at the midpoint of the observed
range rather than at any single measurement; re-tuning them to the newest number
would flip the brief's flag list the next time a refresh lands on a slower host.
Each declaration is therefore the record's measured nominal run time (2 docker
cores, `comment/pipeline/self-validation.json`) with headroom, and the notes
quote ranges rather than one run, so the declaration and the measurement stay in
the same band in both directions: the reviewer flags a declaration that is
more than twice the measured value, and `selfcheck` warns when the measurement
is more than twice the declaration. Read the file for the host those numbers
come from. The 2 to 3 s declarations are the eight standalone checks, each of
which solves one small Hamiltonian; 30 to 45 s are the light grouped replays and
the notebook suite; 110 to 260 s are the four large grouped replays; 750 s is
`examples-scripts`, which runs every official example deck. The declarations sum
to 1572 s against the 6300 s `suite_budget_s`, and the measured nominal suite is
925 s. The remaining gap to the budget is the allowance for a slow or contended
host, and each run reports which side of the budget it landed on in the record's
`budget` field.

## Tolerances

All sixteen checks share one calibration convention: nominal uses `J=1.0,h=0.5`, and the variant moves the active binary64 bond coupling to `J=1.0000000000001` (450 ulps of 1.0, `dJ/J = 1e-13`) while the physical model stays fixed.

The perturbed input is the coupling, not the longitudinal field. Every check builds its spin-chain calibration inside a fixed-magnetization sector, where the field term `h*sum(sigma^z)` is exactly a constant times the identity: the dense matrix of `sum(sigma^z)` has diagonal spread 0 and no off-diagonal entries at every size used here. Moving `h` therefore rescales every level by the same constant instead of perturbing the spectrum, and the graded observable responds only at the eigensolver rounding level. The coupling enters the off-diagonal elements, so perturbing it is a genuine spectral change.

The step is 450 ulps rather than the conventional two. A local probe in the pinned image ran each check three ways — nominal, nominal again, and the variant — and found that a two-ulp coupling change moves the graded observable by `3.6e-15` to `5.3e-14`, at or below the repeat-to-repeat ARPACK noise floor with an unseeded start vector (measured `0` to `6.0e-14` by running the same nominal inputs twice). In 6 of the 8 standalone checks the two-ulp step is no larger than that floor, so it could not be told apart from solver noise. The 450-ulp step moves the observable by `5.6e-13` to `2.5e-11`, which is 28x to 1011x that floor.

The bound stays a floating-point allowance rather than a physics allowance. Across all sixteen checks the recorded spreads are `5.6e-13` to `2.5e-11` (CI run `34990293474`), and each check's worst value uses at most `6.2e-5` of its own `atol + rtol*|x|` bound; the review table's margin column prints the reciprocal of that fraction.

Per-check commands, observables and tolerance rationale live in each check README and rubric.

## Coverage and exclusions

All 73 upstream `test_*.py` files are owned by a check: eight standalone baseline checks plus five grouped checks (`basis-symmetry`, `operators-projections`, `dynamics-utilities`, `entanglement-observables`, `models-crosschecks`). `comment/coverage-matrix.md` lists the file-to-check mapping.

The two families grade differently and the rubrics say so. Each of the eight
baseline checks adapts its upstream file into a self-contained numerical
observable: it builds the same production path the file exercises and grades a
deterministic vector such as a low-lying spectrum or an evolution trace, and its
`default_vs_upstream` states the adaptation. The other 65 `test_*.py` files are
replayed verbatim inside the five grouped checks, under their own upstream
assertions, with the group's calibration energy as the graded value;
`test_basis_particle_sectors.py` is the one file that carries both roles, as the
subject of the `basis-particle-sectors` baseline check and as a member of the
`basis-symmetry` group.

Upstream's own `run_all_tests.sh` also runs two scriptable example suites, and the packaging skill counts an upstream example as an official test, so both are covered too:

- `examples/scripts/` (the `example*.py` glob plus three `user_basis_trivial-*.py` files that upstream documents as examples but its glob skips; all 34 run) by the `examples-scripts` check;
- `sphinx/doc_examples/` (`*example.py`, 33 decks) by the `basis-doc-examples` check;
- `examples/notebooks/` (`*.py`, 6 scripts) by the `examples-notebooks` check.

Both keep upstream's pass condition — the deck must run to completion — and add one spectrum-sensitive calibration observable. The example suites are not redundant with `test/`: four production symbols (`photon.coherent_state`, `operators.commutator`, `operators.anti_commutator`, `tools.misc.get_matvec_function`) are exercised only from the example decks.

One upstream case is excluded and recorded, none silently: `test_quantum_operator.py::test_eigsh` compares two ARPACK `eigsh` outputs by position without sorting, so a correct port can fail it depending on the platform's return order. The reason is repeated in `comment/coverage-matrix.md` and in that check's README.

Two exclusions from earlier revisions of this change were withdrawn after measuring instead of reasoning. `examples/scripts/example11.py` was excluded as scoring outside the check window, but that number came from an arm64 host under emulation; timed natively it runs in 14 s. `examples/scripts/example27.py` was excluded as needing an MKL runtime the image cannot provide, but the `mkl` wheel ships `libmkl_rt.so` inside the virtual environment and the file runs in 42 s once `MKL_RT` points at it. Every file in the official example suites is now covered.

Two upstream files (`test_Op_shift_sector.py`, `test_gen_evolve.py`) carry top-level assertions and no pytest function; the runner detects them and executes them directly so their own assertions decide pass or fail.

Not covered by design: generated documentation prose, the separately released extension source repositories, and platform-specific OpenMP build behaviour. The notebook scripts are covered: the Colab installation block in `quspin_colab.py` is commented out upstream, so the file runs as an ordinary QuSpin script.

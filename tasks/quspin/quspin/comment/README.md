# quspin: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

QuSpin computes exact spectra and time evolution for finite spin, boson and fermion many-body systems. This leaf owns the pinned cohesive package and exercises basis construction, operator assembly, symmetry sectors, sparse Lanczos, Floquet propagation and driven evolution. Documentation, notebooks and the separately released extension source repositories are excluded from runtime checks because they do not add independent solver paths to this pinned package.

## Build

The image installs the pinned Python package and its published extension wheels in a virtual environment. Each check runs against a copied source tree and records zero source-build seconds; the final self-validation record will supply measured run times. The extension wheels remain an explicit provenance caveat for review.

## Tolerances

The first self-validation is a calibration run with nominal and active two-ulp field variants. Its per-check spread will be copied into each rubric before the final self-validation; provisional bounds remain deliberately wider than the measured numerical floor and must be finalized by the curator. Per-check commands, observables and tolerance rationale live in each check README and rubric.

## Blind spots

The initial leaf does not claim exhaustive coverage of every QuSpin test, example, extension workspace or platform-specific OpenMP build. Those omissions are explicit follow-up scope; the selected checks cover the core public basis, operator, sparse-eigensolver and dynamics paths.

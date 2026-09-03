# H2O/cc-pVDZ All-Electron Benchmark Design

## Context

The reviewer of Quantum Harness PR #217 requested a water/cc-pVDZ,
all-electron benchmark without symmetry. For the submitted water geometry,
cc-pVDZ has 24 spatial orbitals and 10 electrons. In the fixed
`N_alpha = N_beta = 5` sector, without point-group symmetry, the determinant
dimension is

`binomial(24, 5)^2 = 1,806,590,016`.

One dense `f64` CI vector is 13.46 GiB. The current direct-FCI operator also
stores the complete diagonal, and the Davidson solver stores multiple complete
vectors. An end-to-end Davidson run would therefore require hundreds of GiB
and is outside the user-approved memory budget.

## Considered Approaches

1. **Report only a combinatorial estimate.** This is safe, but it does not
   answer the reviewer's performance question with executed Rust code.
2. **Run a bounded pipeline and representative sparse Hamiltonian columns.**
   This executes the integral, SCF, transformation, determinant-link, and
   Hamiltonian kernel stages while avoiding every full CI-space allocation.
3. **Implement blocked/out-of-core Davidson.** This could eventually support
   full FCI, but it is a new solver architecture and exceeds the requested
   "a few GiB, not 600 GiB" scope.

The approved design uses approach 2.

## Benchmark Contract

The benchmark uses the existing challenge water geometry in Angstrom:

```text
O  0                    0                   0
H  0.967                0                   0
H -0.2923916843556798   0.9217353757557798 0
```

It uses the named `cc-pVDZ` basis, all 10 electrons, `MS2 = 0`, and no
point-group symmetry. Fixing electron number and `MS2` is not point-group
symmetry reduction.

The executable stages are:

1. libcint AO-integral construction;
2. all-electron RHF;
3. four-index AO-to-MO transformation;
4. alpha and beta occupation-string/link-table construction;
5. a configurable number of sparse Hamiltonian source columns.

The command reports stage timings, determinant counts, link counts, sparse
column nonzeros and checksum, RHF accuracy against the committed PySCF
reference, and exact dense-vector/Davidson memory estimates.

## Memory Safety

The benchmark must never instantiate `DirectFciOperator` for this system and
must never allocate a vector whose length is the determinant dimension.
Instead, a diagonal-free direct-FCI kernel owns only:

- the 24-orbital integral tensors;
- the 42,504 alpha and 42,504 beta strings;
- their one-body link tables;
- one sparse destination accumulator for one source column at a time.

The expected resident set is below a few GiB. A command-line memory budget,
defaulting to 2 GiB, rejects the run before link construction if the
conservative resident-memory estimate exceeds that budget.

## Output and Reproducibility

The command writes a versioned JSON result containing the complete physical
input, reference energy, timings, counts, memory estimates, and kernel
checksum. Human-readable output exposes the same fields.

The committed report records:

- repository commit and optimized build command;
- Rust/compiler, operating system, processor, RAM, and thread count;
- repeated-run policy;
- peak resident memory measured by the operating system;
- the explicit statement that no converged full-FCI energy is claimed.

## Verification

Unit tests prove the combinatorial counts and memory estimates. On a small
FCIDUMP fixture, the sparse source-column kernel must reproduce the matching
column from the existing full direct-FCI operator. The cc-pVDZ live benchmark
must converge RHF and match the PySCF 2.14.0 reference
`-76.025792594904772 Eh` within `1e-8 Eh`.

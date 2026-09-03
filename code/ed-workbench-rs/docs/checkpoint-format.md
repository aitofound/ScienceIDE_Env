# Davidson checkpoint format

## Purpose and boundary

The v0.3 Davidson workspace moves the basis and sigma subspace from resident
`Vec<Vec<f64>>` storage to versioned files on local disk. It supports
interruption and resume while keeping a bounded number of full vectors in
memory.

It does not make a calculation possible when one full vector cannot fit in
RAM, and it does not claim converged all-electron H2O/cc-pVDZ full FCI.

## Workspace layout

```text
workspace/
├── checkpoint.json
├── basis/
│   ├── generation-000000/
│   │   ├── vector-000000.bin
│   │   └── ...
│   └── generation-000001/
│       └── ...
├── sigma/
│   └── generation-000000/
│       └── ...
└── results/
    ├── result-000001.bin
    └── ...
```

Each vector file contains exactly `dimension` IEEE-754 binary64 values in
little-endian order. There is no header inside a vector file; all metadata is
in `checkpoint.json`.

## Manifest schema version 1

| Field | Meaning |
|---|---|
| `schema_version` | Exactly `1` |
| `operator_fingerprint` | Caller-supplied identity; CLI default is the FCIDUMP SHA-256 |
| `dimension` | Full determinant-vector length |
| `residual_tolerance` | Davidson residual convergence threshold |
| `energy_tolerance` | Davidson Ritz-energy convergence threshold |
| `max_subspace` | Restart boundary |
| `completed_iterations` | Last fully committed iteration |
| `previous_energy` | Ritz energy used for the next energy-change test |
| `basis_generation` | Active basis generation directory |
| `basis_count` | Committed basis vectors |
| `sigma_generation` | Active sigma generation directory |
| `sigma_count` | Committed sigma vectors |
| `last_energy` | Last Ritz energy |
| `last_residual_norm` | Last residual norm |
| `last_converged` | Whether the saved result met convergence |
| `result_vector_file` | Relative path to the last Ritz vector |
| `scalar_type` | Exactly `f64` |
| `byte_order` | Exactly `little` |

`max_iterations` and checkpoint cadence are deliberately not compatibility
keys. A run stopped at iteration 5 may resume with a larger maximum iteration
count or a different save cadence. Dimension, fingerprint, tolerances, and
subspace size must match exactly.

## Commit protocol

1. Newly appended vectors are written to temporary files, flushed, synced,
   and renamed.
2. A Davidson restart writes a new generation directory; the previously
   committed generation is not modified.
3. The latest Ritz vector is written and synced.
4. The manifest is serialized to `checkpoint.json.tmp`, flushed, synced, and
   renamed to `checkpoint.json`.

An interrupted write therefore leaves either the previous committed manifest
or the new one. Extra unreferenced files may remain and are ignored. The
implementation does not recursively clean a user-supplied workspace.

## Validation on resume

Resume rejects:

- invalid JSON or an unsupported schema;
- an absolute or parent-traversing result path;
- fingerprint, dimension, tolerance, or subspace mismatch;
- unequal or empty basis/sigma counts;
- missing, truncated, oversized, or non-finite vector files;
- scalar type or byte-order mismatch.

All validation happens before a new Davidson iteration begins.

## CLI

Start a disk-backed run:

```bash
cargo run --release --locked -- davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --workspace /path/to/h2o-workspace \
  --checkpoint-every 1 \
  --memory-budget-gib 2 \
  --residual-tolerance 1e-7 \
  --max-iterations 60 \
  --max-subspace 20
```

Resume it:

```bash
cargo run --release --locked -- davidson \
  fixtures/h2o-631g-fc/FCIDUMP \
  --workspace /path/to/h2o-workspace \
  --resume \
  --checkpoint-every 2 \
  --memory-budget-gib 2 \
  --residual-tolerance 1e-7 \
  --max-iterations 100 \
  --max-subspace 20
```

The CLI uses the FCIDUMP SHA-256 as the operator fingerprint unless
`--operator-fingerprint` is supplied. Overriding it transfers responsibility
for Hamiltonian identity to the caller.

## Memory estimate

The CLI preflight estimates:

```text
disk workspace:  7 × dimension × 8 bytes
memory storage:  (2 × max_subspace + 6) × dimension × 8 bytes
```

This is a conservative solver-vector estimate, not an operating-system hard
limit and not total process RSS. The operator, determinant links, integrals,
allocator overhead, linear-algebra workspaces, and filesystem cache require
additional memory.

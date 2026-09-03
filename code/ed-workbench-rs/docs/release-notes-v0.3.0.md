# v0.3.0 — Restartable disk-backed Davidson

v0.3.0 adds a versioned single-node Davidson workspace. The existing
`lowest_eigenpair` API and default in-memory CLI path remain available.

## New capabilities

- basis and sigma vectors can live in a local NVMe workspace;
- every committed checkpoint contains a versioned JSON manifest;
- vector files use explicit little-endian `f64` representation;
- interrupted runs resume with cumulative iteration state;
- the FCIDUMP SHA-256 identifies the operator by default;
- incompatible, corrupt, truncated, oversized, or non-finite state is
  rejected before iteration;
- restart generations and the manifest use an atomic commit protocol;
- a conservative vector-memory preflight runs before Davidson allocation.

The disk algorithm loads one stored vector at a time during projection,
Ritz-vector assembly, orthogonalization, and restart. Its resident vector
count does not grow with the complete Davidson subspace.

## Compatibility

The original call remains unchanged:

```rust
let result = lowest_eigenpair(&operator, &initial, &config)?;
```

Disk-backed execution is explicit:

```rust
let result = lowest_eigenpair_with_run_config(
    &operator,
    &initial,
    &DavidsonRunConfig {
        algorithm: config,
        workspace: Some(DavidsonWorkspaceConfig {
            path: workspace.into(),
            resume: false,
            checkpoint_every: 1,
            operator_fingerprint: fingerprint,
        }),
    },
)?;
```

An integration test interrupts a five-dimensional calculation after one
iteration, resumes it, and matches the uninterrupted in-memory energy,
residual, and phase-aligned eigenvector.

## Scope boundary

Out-of-core subspace storage is not distributed FCI. One complete determinant
vector and several working vectors must still fit in RAM. H2O/cc-pVDZ
all-electron has 1,806,590,016 determinants and a single `f64` vector requires
13.460145 GiB; this release continues to treat that system as a bounded
resource/kernel benchmark rather than claiming a converged full-FCI energy.

See [the checkpoint format](checkpoint-format.md) for schema, commit protocol,
validation rules, CLI examples, and memory equations.

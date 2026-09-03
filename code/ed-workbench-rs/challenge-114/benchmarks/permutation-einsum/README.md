# Benchmark Family: Permutation-Heavy Einsum

## Purpose

Measure layouts and contractions where data movement dominates arithmetic.

## Initial Cases

| Case | Description | Reference backends |
|---|---|---|
| permute-contract-small | transpose/permutation followed by small contraction | PyTorch, JAX, tenferro-rs |
| batched-small-contract | repeated small matrix contractions | PyTorch, JAX, tenferro-rs, faer |
| signed-index-accumulation | sign-flipped indexed accumulation | Rust loop, tenferro-rs if expressible |

## Output Contract

Each case should produce timing data and a correctness comparison against an
independent reference implementation.


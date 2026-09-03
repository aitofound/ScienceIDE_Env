# Benchmark Family: Eager Small Loops

## Purpose

Measure dispatch and memory overhead in many repeated small tensor operations.

## Initial Cases

| Case | Description | Reference backends |
|---|---|---|
| dot-axpy-loop | repeated dot, norm, and axpy | Rust loop, faer, tenferro-rs |
| denom-update-loop | residual divided by denominator vector | Rust loop, tenferro-rs |
| masked-update-loop | repeated masked element-wise update | Rust loop, tenferro-rs if expressible |

## Output Contract

Each case should report median runtime, correctness error, and hardware profile.

